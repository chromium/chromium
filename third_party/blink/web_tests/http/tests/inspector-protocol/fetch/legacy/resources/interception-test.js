// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(class InterceptionHelper {
  constructor(testRunner, session) {
    this._testRunner = testRunner;
    this._session = session;
    this._interceptionRequestParams = {};
    this._requestIdToFilename = {};
    this._filenameToInterceptionId = {};
    this._loggedMessages = {};
    this._consoleLogs = [];
    this._idToCanoncalId = {};
    this._nextId = 1;
  }

  _getNextId() {
    return 'ID ' + this._nextId++;
  }

  _canonicalId(id) {
    if (!this._idToCanoncalId.hasOwnProperty(id))
      this._idToCanoncalId[id] = this._getNextId();
    return this._idToCanoncalId[id];
  }

  _log(id, message) {
    if (!this._loggedMessages.hasOwnProperty(id))
      this._loggedMessages[id] = [];
    this._loggedMessages[id].push(message);
  }

  _completeTest(message) {
    // The order in which network events occur is not fully deterministic so we
    // sort based on the interception ID to try and make the test non-flaky.
    for (var property in this._loggedMessages) {
      if (this._loggedMessages.hasOwnProperty(property)) {
        var messages = this._loggedMessages[property];
        for (var i = 0; i < messages.length; i++)
          this._testRunner.log(messages[i]);
      }
    }
    for (var consoleLog of this._consoleLogs)
      this._testRunner.log(consoleLog);
    if (message)
      this._testRunner.log(message);
    this._testRunner.completeTest();
  }

  setSilentFrameStoppedLoading(silent = true) {
    this.silentFrameStoppedLoading = silent;
  }

  async startInterceptionTest(requestInterceptedDict, numConsoleLogsToWaitFor,
                              interceptionStage = 'Request') {
    if (typeof numConsoleLogsToWaitFor === 'undefined')
      numConsoleLogsToWaitFor = 0;
    var frameStoppedLoading = false;

    var getInterceptionId = filename => {
      if (!this._filenameToInterceptionId.hasOwnProperty(filename))
        this._filenameToInterceptionId[filename] = this._getNextId();
      return this._filenameToInterceptionId[filename];
    };

    // Wait until we've seen Page.frameStoppedLoading and the expected number of
    // console logs.
    var maybeCompleteTest = () => {
      if (numConsoleLogsToWaitFor === 0 && frameStoppedLoading)
        this._completeTest();
    };

    this._session.protocol.Fetch.onAuthRequired(event => {
      var filename = event.params.request.url.split('/').pop();
      var id = this._canonicalId(event.params.requestId);
      this._filenameToInterceptionId[filename] = id;
      if (!requestInterceptedDict.hasOwnProperty(filename + '+Auth')) {
        this._completeTest('FAILED: unexpected auth challenge ' +
                           JSON.stringify(event.params));
        return;
      }
      this._log(id, 'Auth required for ' + id);
      requestInterceptedDict[filename + '+Auth'](event);
    });

    this._session.protocol.Fetch.onRequestPaused(event => {
      const filename = event.params.request.url.split('/').pop();
      const id = this._canonicalId(event.params.requestId);
      this._filenameToInterceptionId[filename] = id;
      if (!requestInterceptedDict.hasOwnProperty(filename)) {
        this._completeTest('FAILED: unexpected request interception ' +
                           JSON.stringify(event.params));
        return;
      }
      if (!this._interceptionRequestParams[id])
        this._interceptionRequestParams[id] = event.params.request;
      let redirectUrl;
      for (var h of event.params.responseHeaders || []) {
        if (h.name.toLowerCase() === 'location')
          redirectUrl = h.value;
      }
      if (event.params.responseStatusCode &&
          event.params.responseStatusCode >= 300 &&
          event.params.responseStatusCode < 400 && redirectUrl) {
        event.params.redirectUrl = redirectUrl;
        var errorReason = '';
        if (event.params.responseErrorReason)
          errorReason = event.params.responseErrorReason + ' ';
        this._log(id,
                  'Network.requestIntercepted ' + id + ' ' + errorReason +
                      event.params.responseStatusCode + ' redirect ' +
                      this._interceptionRequestParams[id].url.split('/').pop() +
                      ' -> ' + event.params.redirectUrl.split('/').pop());
        this._interceptionRequestParams[id].url = event.params.redirectUrl;
      } else {
        this._interceptionRequestParams[id] = event.params.request;
        this._log(id,
                  'Network.requestIntercepted ' + id + ' ' +
                      event.params.request.method + ' ' + filename +
                      ' type: ' + event.params.resourceType);
      }
      requestInterceptedDict[filename](event);
    });

    this._session.protocol.Network.onLoadingFailed(event => {
      var filename = this._requestIdToFilename[event.params.requestId];
      var id = getInterceptionId(filename);
      this._log(
          id,
          'Network.loadingFailed ' + filename + ' ' + event.params.errorText);
    });

    this._session.protocol.Network.onRequestWillBeSent(event => {
      var filename = event.params.request.url.split('/').pop();
      this._requestIdToFilename[event.params.requestId] = filename;
    });

    this._session.protocol.Network.onResponseReceived(event => {
      var response = event.params.response;
      var filename = response.url.split('/').pop();
      var id = getInterceptionId(filename);
      this._log(id,
                'Network.responseReceived ' + filename + ' ' + response.status +
                    ' ' + response.mimeType);
    });

    this._session.protocol.Runtime.onConsoleAPICalled(messageObject => {
      if (messageObject.params.type !== 'log')
        return;
      this._consoleLogs.push(messageObject.params.args[0].value);
      numConsoleLogsToWaitFor--;
      maybeCompleteTest();
    });

    this._session.protocol.Page.onFrameStoppedLoading(() => {
      // We want to see errors that might stop frame loading, so we delay
      // completion a bit.
      setTimeout(() => {
        frameStoppedLoading = true;
        if (!this.silentFrameStoppedLoading) {
          this._log(this._getNextId(), 'Page.frameStoppedLoading');
        }
        maybeCompleteTest();
      }, 0);
    });

    this._testRunner.log('Test started');
    await this._session.protocol.Network.clearBrowserCookies();
    await this._session.protocol.Network.clearBrowserCache();
    await this._session.protocol.Network.setCacheDisabled(
        {cacheDisabled: true});
    this._session.protocol.Network.enable();
    this._testRunner.log('Network agent enabled');
    var patterns = [];
    if (interceptionStage === 'HeadersReceived' || interceptionStage === 'Both')
      patterns.push({urlPattern: '*', requestStage: 'Response'});
    if (interceptionStage === undefined || interceptionStage === 'Request' ||
        interceptionStage === 'Both')
      patterns.push({urlPattern: '*', requestStage: 'Request'});
    await this._session.protocol.Fetch.enable(
        {patterns: patterns, handleAuthRequests: true});
    this._testRunner.log('Request interception enabled');
    await this._session.protocol.Page.enable();
    this._testRunner.log('Page agent enabled');
    await this._session.protocol.Runtime.enable();
    this._testRunner.log('Runtime agent enabled');
  }

  allowRequest(event) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, 'allowRequest ' + id);
    var params = {requestId: event.params.requestId};
    this._session.protocol.Fetch.continueRequest(params);
  }

  modifyRequest(event, params) {
    var id = this._canonicalId(event.params.requestId);
    var mods = [];
    for (var property in params) {
      if (!params.hasOwnProperty(property))
        continue;
      if (property === 'url') {
        var newUrl = params['url'];
        var filename = this._interceptionRequestParams[id].url;
        mods.push('url ' + filename.split('/').pop() + ' -> ' + newUrl);
        var directoryPath =
            filename.substring(0, filename.lastIndexOf('/') + 1);
        params['url'] = directoryPath + newUrl;
      } else {
        mods.push(
            property + ' ' +
            JSON.stringify(this._interceptionRequestParams[id][property]) +
            ' -> ' + JSON.stringify(params[property]));
      }
    }

    this._log(id, 'modifyRequest ' + id + ': ' + mods.join('; '));
    params['requestId'] = event.params.requestId;
    if (params.postData)
      params.postData = btoa(params.postData);
    if (params.responseCode || params.responseHeaders || params.body) {
      this._session.protocol.Fetch.fulfillRequest({
        requestId: event.params.requestId,
        responseCode: params.responseCode || 200,
        responsePhrase: params.responsePhrase || 'OK',
        responseHeaders: params.responseHeaders || [],
        body: params.body !== undefined ? btoa(params.body) : undefined
      });
      return;
    }
    this._session.protocol.Fetch.continueRequest(params);
  }

  blockRequest(event, errorReason) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, 'blockRequest ' + id + ' ' + errorReason);
    this._session.protocol.Fetch.failRequest(
        {requestId: event.params.requestId, errorReason: errorReason});
  }

  mockResponse(event, {
    responseCode = 200,
    responsePhrase = 'OK',
    responseHeaders = [],
    body = ''
  }) {
    const id = this._canonicalId(event.params.requestId);
    this._log(id, 'mockResponse ' + id);
    const fulfillParams = {
      requestId: event.params.requestId,
      responseCode,
      responsePhrase,
      responseHeaders
    };
    fulfillParams.body = btoa(body);
    this._session.protocol.Fetch.fulfillRequest(fulfillParams);
  }

  disableRequestInterception(event) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, '----- disableRequestInterception -----');
    this._session.protocol.Fetch.disable();
    this._session.protocol.Fetch
        .continueRequest({requestId: event.params.requestId})
        .catch(() => {});
  }

  cancelAuth(event) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, '----- Cancel Auth -----');
    this._session.protocol.Fetch.continueWithAuth({
      requestId: event.params.requestId,
      authChallengeResponse: {response: 'CancelAuth'}
    });
  }

  defaultAuth(event) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, '----- Use Default Auth -----');
    this._session.protocol.Fetch.continueWithAuth({
      requestId: event.params.requestId,
      authChallengeResponse: {response: 'Default'}
    });
  }

  provideAuthCredentials(event, username, password) {
    var id = this._canonicalId(event.params.requestId);
    this._log(id, '----- Provide Auth Credentials -----');
    this._session.protocol.Fetch.continueWithAuth({
      requestId: event.params.requestId,
      authChallengeResponse: {
        response: 'ProvideCredentials',
        username: username,
        password: password
      }
    });
  }
})
