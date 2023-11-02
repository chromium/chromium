// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to allow renderer tests to conveniently intercept network
 * requests and provide HTTP headers and body.
 */
(class HttpInterceptor {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {!Page} page TestRunner.Page instance.
   */
  constructor(testRunner, dp) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.disabledRequestedUrlsLogging = false;
    this.responses_ = new Map();
    this.requestedUrls_ = [];
    this.requestedMethods_ = [];
  }

  /**
   * Initializes the helper returning reference to itself to allow assignment.
   *
   * @return {!object} HttpInterceptor reference.
   */
  async init() {
    await this.dp_.Network.enable();
    await this.dp_.Network.setRequestInterception(
        { patterns: [{ urlPattern: '*' }] });

    this.dp_.Network.onRequestIntercepted(event => {
      const method = event.params.request.method;
      this.requestedMethods_.push(method);

      const url = event.params.request.url
          + (event.params.request.urlFragment || '');
      this.requestedUrls_.push(url);

      var response = this.responses_.get(url);
      if (response) {
        if (!this.disabledRequestedUrlsLogging) {
          this.testRunner_.log(`requested url: ${url}`);
        }
      } else {
        this.testRunner_.log(`requested url: ${url} is not known`);
        this.logResponses();
      }
      const body = (response && response.body) || '';
      const headers = (response && response.headers) || [];
      const headers_with_body = headers.join('\r\n') + '\r\n\r\n' + body;
      this.dp_.Network.continueInterceptedRequest({
        interceptionId: event.params.interceptionId,
        rawResponse: btoa(headers_with_body)
      });
    });

    return this;
  }

  /**
   * Prevents requested urls from being logged. This helps to stabilize tests
   * that request urls in arbitrary order. Use hasRequestedUrls(urls) function
   * to check if expected urls has been requested.
   *
   * @param {boolean} value True if requested url logging is disabled, false
   *     otherwise.
   */
  setDisableRequestedUrlsLogging(value) {
    this.disabledRequestedUrlsLogging = value;
  }

  /**
   * Adds request response.
   *
   * @param {!string} url Request url, including #fragment.
   * @param {?string} body Request response body, optional.
   * @param {?[string]} headers Request response headers, optional.
   */
  addResponse(url, body, headers) {
    this.responses_.set(url, {body, headers});
  }

  /**
   * Logs registered request responses.
   */
  logResponses() {
    this.testRunner_.log(`Responses: ${this.responses_.size}`);
    for (const [url, value] of this.responses_.entries()) {
      this.testRunner_.log(
          `url=${url}\nbody=${value.body}\nheaders=${value.headers}`);
    }
  }

  /**
   * Logs requested methods in the order requests have been received.
   */
  logRequestedMethods() {
    this.testRunner_.log(`Requested methods: ${this.requestedMethods_.length}`);
    for (const method of this.requestedMethods_) {
      this.testRunner_.log(` ${method}`);
    }
  }

  /**
   * Logs requested urls in the order requests have been received.
   */
  logRequestedUrls() {
    this.testRunner_.log(`Requested urls: ${this.requestedUrls_.length}`);
    for (const url of this.requestedUrls_) {
      this.testRunner_.log(` ${url}`);
    }
  }

  /**
   * Checks if specified urls have been requested.
   *
   * @param {[string]} urls Array of urls to check against requested urls.
   */
  hasRequestedUrls(urls) {
    this.testRunner_.log(`Expected requested urls:`);
    for (const url of urls) {
      if (this.requestedUrls_.indexOf(url) >= 0) {
        this.testRunner_.log(` ${url}`);
      } else {
        this.testRunner_.log(` ${url} is MISSING`);
      }
    }
  }

});
