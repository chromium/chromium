// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

class FetchHandler {
  constructor(testRunner, protocol, once) {
    this._testRunner = testRunner;
    this._protocol = protocol;
    this._callback = null;
    this._once = once;
  }

  _handle(params) {
    this._callback(params);
  }

  matched() {
    return new Promise(fulfill => this._callback = fulfill);
  }

  async continueRequest(params) {
    for (;;) {
      const request = await this.matched();
      const result = this._protocol.Fetch.continueRequest(
          Object.assign(params || {}, {requestId: request.requestId}))
              .then(result => this._handleError(result));
      if (this._once)
        return result;
    }
  }

  async fail(params) {
    for (;;) {
      const request = await this.matched();
      const result = this._protocol.Fetch.failRequest(
        Object.assign(params, {requestId: request.requestId}))
            .then(result => this._handleError(result));
      if (this._once)
        return result;
    }
  }

  async fulfill(params) {
    for (;;) {
      const request = await this.matched();
      const result = this._protocol.Fetch.fulfillRequest(
          Object.assign(params, {requestId: request.requestId}))
              .then(result => this._handleError(result));
      if (this._once)
        return result;
    }
  }

  _handleError(result) {
    if (result.error && !/Invalid InterceptionId/.test(result.error.message))
      this._testRunner.log(`Got error: ${result.error.message}`);
  }
};

class FetchHelper {
  constructor(testRunner, targetProtocol) {
    this._handlers = [];
    this._onceHandlers = [];
    this._testRunner = testRunner;
    this._protocol = targetProtocol;
    this._logPrefix = '';
    this._enableLogging = true;
    this._protocol.Fetch.onRequestPaused(event => {
      this._logRequest(event);
      const handler = this._findHandler(event);
      if (handler)
        handler._handle(event);
    });
  }

  enable() {
    return this._protocol.Fetch.enable({});
  }

  onRequest(pattern) {
    const handler = new FetchHandler(this._testRunner, this._protocol, false);
    this._handlers.push({pattern, handler});
    return handler;
  }

  onceRequest(pattern) {
    const handler = new FetchHandler(this._testRunner, this._protocol, true);
    this._onceHandlers.push({pattern, handler});
    return handler;
  }

  setLogPrefix(logPrefix) {
    this._logPrefix = logPrefix || '';
  }

  setEnableLogging(enableLogging) {
    this._enableLogging = typeof enableLogging === 'undefined' ? true : enableLogging;
  }

  static makeHeaders(headers) {
    const result = [];
    for (const header of headers) {
      const kv = header.split(":");
      result.push({ name: kv[0].trim(), value: kv[1].trim()});
    }
    return result;
  }

  static makeResponse(body, headers, code) {
    const response = {
      responseCode: code || 200,
      responseHeaders: this.makeHeaders(headers || ["Content-type: text/html"]),
    }

    if (body)
      response["body"] = btoa(body);

    return response;
  }

  static makeContentResponse(body, contentType) {
    return this.makeResponse(body,
        contentType ? ["Content-type: " + contentType] : contentType);
  }

  static makeRedirectResponse(location) {
    return this.makeResponse(undefined, ["Location: " + location], 302);
  }

  _logRequest(event) {
    if (!this._enableLogging) return;
    const params = event.params;
    const response = event.responseErrorReason || event.responseStatusCode;
    const response_text = response ? 'Response' : 'Request';
    this._testRunner.log(`${this._logPrefix}${response_text} to ${params.request.url}, type: ${params.resourceType}`);
  }

  _findHandler(event) {
    const params = event.params;
    const url = params.request.url;
    let entry;
    let index = FetchHelper._findHandlerIndex(this._onceHandlers, url);
    if (index >= 0) {
      [entry] = this._onceHandlers.splice(index, 1);
    } else {
      index = FetchHelper._findHandlerIndex(this._handlers, url);
      if (index >= 0)
        entry = this._handlers[index];
    }
    if (entry)
      entry.handler._handle(params);
  }

  static _findHandlerIndex(arr, url) {
    return arr.findIndex(item => {
      if (!item.pattern)
        return true;
      if (typeof item.pattern === 'string' || item.pattern instanceof String) {
        return url === item.pattern;
      }
      return item.pattern.test(url);
    });
  }

};

return FetchHelper;
})()
