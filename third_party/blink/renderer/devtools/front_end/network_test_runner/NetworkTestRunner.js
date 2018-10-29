// Copyright 2017 The Chromium Authors. All
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview using private properties isn't a Closure violation in tests.
 * @suppress {accessControls}
 */

NetworkTestRunner.waitForRequestResponse = function(request) {
  if (request.responseReceivedTime !== -1)
    return Promise.resolve(request);

  return TestRunner.waitForEvent(
      SDK.NetworkManager.Events.RequestUpdated, TestRunner.networkManager,
      updateRequest => updateRequest === request && request.responseReceivedTime !== -1);
};

NetworkTestRunner.waitForNetworkLogViewNodeForRequest = function(request) {
  const networkLogView = UI.panels.network._networkLogView;
  const node = networkLogView.nodeForRequest(request);

  if (node)
    return Promise.resolve(node);

  console.assert(networkLogView._staleRequests.has(request));

  return TestRunner.addSnifferPromise(networkLogView, '_didRefreshForTest').then(() => {
    const node = networkLogView.nodeForRequest(request);
    console.assert(node);
    return node;
  });
};

NetworkTestRunner.waitForWebsocketFrameReceived = function(wsRequest, message) {
  for (const frame of wsRequest.frames()) {
    if (checkFrame(frame))
      return Promise.resolve(frame);
  }

  return TestRunner.waitForEvent(SDK.NetworkRequest.Events.WebsocketFrameAdded, wsRequest, checkFrame);

  function checkFrame(frame) {
    return frame.type === SDK.NetworkRequest.WebSocketFrameType.Receive && frame.text === message;
  }
};

NetworkTestRunner.recordNetwork = function() {
  UI.panels.network._networkLogView.setRecording(true);
};

NetworkTestRunner.networkWaterfallColumn = function() {
  return UI.panels.network._networkLogView._columns._waterfallColumn;
};

NetworkTestRunner.networkRequests = function() {
  return Array.from(SDK.networkLog.requests());
};

NetworkTestRunner.dumpNetworkRequests = function() {
  const requests = NetworkTestRunner.networkRequests();

  requests.sort(function(a, b) {
    return a.url().localeCompare(b.url());
  });

  TestRunner.addResult('resources count = ' + requests.length);

  for (i = 0; i < requests.length; i++)
    TestRunner.addResult(requests[i].url());
};

NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo = function() {
  for (const request of SDK.networkLog.requests()) {
    TestRunner.addResult(`* ${request.url()}`);
    TestRunner.addResult(`  failed: ${!!request.failed}`);
    TestRunner.addResult(`  statusCode: ${request.statusCode}`);
    TestRunner.addResult(`  resourceType: ${request.resourceType().name()}`);
    if (request.signedExchangeInfo()) {
      TestRunner.addResult('  SignedExchangeInfo');
      if (request.signedExchangeInfo().header) {
        const header = request.signedExchangeInfo().header;
        TestRunner.addResult(`    Request URL: ${header.requestUrl}`);
        for (const signature of header.signatures)
          TestRunner.addResult(`    Certificate URL: ${signature.certUrl}`);
      }
      if (request.signedExchangeInfo().securityDetails) {
        const securityDetails = request.signedExchangeInfo().securityDetails;
        TestRunner.addResult(`    Certificate Subject: ${securityDetails.subjectName}`);
        TestRunner.addResult(`    Certificate Issuer: ${securityDetails.issuer}`);
      }
      if (request.signedExchangeInfo().errors) {
        for (const errorMessage of request.signedExchangeInfo().errors)
          TestRunner.addResult(`    Error: ${JSON.stringify(errorMessage)}`);
      }
    }
  }
};

NetworkTestRunner.findRequestsByURLPattern = function(urlPattern) {
  return NetworkTestRunner.networkRequests().filter(function(value) {
    return urlPattern.test(value.url());
  });
};

NetworkTestRunner.makeSimpleXHR = function(method, url, async, callback) {
  NetworkTestRunner.makeXHR(method, url, async, undefined, undefined, [], false, undefined, undefined, callback);
};

NetworkTestRunner.makeSimpleXHRWithPayload = function(method, url, async, payload, callback) {
  NetworkTestRunner.makeXHR(method, url, async, undefined, undefined, [], false, payload, undefined, callback);
};

NetworkTestRunner.makeXHR = function(
    method, url, async, user, password, headers, withCredentials, payload, type, callback) {
  const args = {};
  args.method = method;
  args.url = TestRunner.url(url);
  args.async = async;
  args.user = user;
  args.password = password;
  args.headers = headers;
  args.withCredentials = withCredentials;
  args.payload = payload;
  args.type = type;
  const jsonArgs = JSON.stringify(args).replace(/\"/g, '\\"');

  function innerCallback(msg) {
    if (msg.messageText.indexOf('XHR loaded') !== -1) {
      if (callback)
        callback();
    } else {
      ConsoleTestRunner.addConsoleSniffer(innerCallback);
    }
  }

  ConsoleTestRunner.addConsoleSniffer(innerCallback);
  TestRunner.evaluateInPageAnonymously('makeXHRForJSONArguments("' + jsonArgs + '")');
};

NetworkTestRunner.makeFetch = function(url, requestInitializer, callback) {
  TestRunner.callFunctionInPageAsync('makeFetch', [url, requestInitializer]).then(callback);
};

NetworkTestRunner.makeFetchInWorker = function(url, requestInitializer, callback) {
  TestRunner.callFunctionInPageAsync('makeFetchInWorker', [url, requestInitializer]).then(callback);
};

NetworkTestRunner.clearNetworkCache = function() {
  // This turns cache off and then on, effectively clearning the memory cache.
  return Promise.all([
    TestRunner.NetworkAgent.clearBrowserCache(),
    TestRunner.NetworkAgent.setCacheDisabled(true).then(() => TestRunner.NetworkAgent.setCacheDisabled(false))
  ]);
};

NetworkTestRunner.HARPropertyFormatters = {
  bodySize: 'formatAsTypeName',
  compression: 'formatAsTypeName',
  connection: 'formatAsTypeName',
  headers: 'formatAsTypeName',
  headersSize: 'formatAsTypeName',
  id: 'formatAsTypeName',
  onContentLoad: 'formatAsTypeName',
  onLoad: 'formatAsTypeName',
  receive: 'formatAsTypeName',
  startedDateTime: 'formatAsRecentTime',
  time: 'formatAsTypeName',
  timings: 'formatAsTypeName',
  version: 'formatAsTypeName',
  wait: 'formatAsTypeName',
  _transferSize: 'formatAsTypeName',
  _error: 'skip',
  _initiator: 'formatAsTypeName',
  _priority: 'formatAsTypeName'
};

NetworkTestRunner.HARPropertyFormattersWithSize = JSON.parse(JSON.stringify(NetworkTestRunner.HARPropertyFormatters));
NetworkTestRunner.HARPropertyFormattersWithSize.size = 'formatAsTypeName';

TestRunner.deprecatedInitAsync(`
  let lastXHRIndex = 0;

  function xhrLoadedCallback() {
    console.log('XHR loaded: ' + ++lastXHRIndex);
  }

  function makeSimpleXHR(method, url, async, callback) {
    makeSimpleXHRWithPayload(method, url, async, null, callback);
  }

  function makeSimpleXHRWithPayload(method, url, async, payload, callback) {
    makeXHR(method, url, async, undefined, undefined, [], false, payload, undefined, callback);
  }

  function makeXHR(method, url, async, user, password, headers, withCredentials, payload, type, callback) {
    let xhr = new XMLHttpRequest();

    if (type == undefined)
      xhr.responseType = '';
    else
      xhr.responseType = type;

    xhr.onreadystatechange = function() {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        if (typeof callback === 'function')
          callback();
      }
    };

    xhr.open(method, url, async, user, password);
    xhr.withCredentials = withCredentials;

    for (let i = 0; i < headers.length; ++i)
      xhr.setRequestHeader(headers[i][0], headers[i][1]);

    try { xhr.send(payload); } catch (e) {}
  }

  function makeXHRForJSONArguments(jsonArgs) {
    let args = JSON.parse(jsonArgs);

    makeXHR(
      args.method,
      args.url,
      args.async,
      args.user,
      args.password,
      args.headers || [],
      args.withCredentials,
      args.payload,
      args.type,
      xhrLoadedCallback
    );
  }

  function makeFetch(url, requestInitializer) {
    return fetch(url, requestInitializer).then(res => {
      res.text();
      return res;
    }).catch(e => e);
  }

  function makeFetchInWorker(url, requestInitializer) {
    return new Promise(resolve => {
      let worker = new Worker('/devtools/network/resources/fetch-worker.js');

      worker.onmessage = event => {
        resolve(event.data);
      };

      worker.postMessage({
        url: url,
        init: requestInitializer
      });
    });
  }
`);
