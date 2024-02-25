// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult("Test to ensure the consistency of front-end patterns vs backend patterns for request interception.\n");

  // Backend supports wildcards, but front-end does not. This test is to ensure the basic stability with wildcard characters.
  var urlPrefix = SDK.TargetManager.TargetManager.instance().primaryPageTarget().inspectedURL().replace(/\/[^\/]*$/, '');
  var resourceURL = urlPrefix + '/bar.js';
  await checkPattern('**bar.js');
  await checkPattern(resourceURL);
  await checkPattern('*bar.js');
  await checkPattern('bar.js');
  await checkPattern('\*bar.js');
  await checkPattern('*b*');
  await checkPattern('*');
  await checkPattern('*bar_js');
  await checkPattern('*bar?js');

  TestRunner.completeTest();

  /**
   * @param {string} pattern
   * @return {!Promise}
   */
  async function checkPattern(pattern) {
    TestRunner.addResult("Setting Pattern: " + cleanURLOrPattern(pattern));
    await SDK.NetworkManager.MultitargetNetworkManager.instance().setInterceptionHandlerForPatterns([{urlPattern: pattern}], interceptionHandler);
    TestRunner.addResult("Requesting: " + cleanURLOrPattern(resourceURL));
    await TestRunner.evaluateInPageAsync(`fetch('` + resourceURL + `')`);
    TestRunner.addResult("Response Received: " + cleanURLOrPattern(resourceURL));
    await SDK.NetworkManager.MultitargetNetworkManager.instance().setInterceptionHandlerForPatterns([], interceptionHandler);
    TestRunner.addResult("");

    /**
     * @param {!SDK.NetworkManager.InterceptedRequest} interceptedRequest
     * @return {!Promise}
     */
    function interceptionHandler(interceptedRequest) {
      TestRunner.addResult("Intercepted Request: " + cleanURLOrPattern(interceptedRequest.request.url));
      return Promise.resolve();
    }
  }

  /**
   * @param {string} url
   * @return {string}
   */
  function cleanURLOrPattern(urlOrPattern) {
    if (urlOrPattern.startsWith(urlPrefix))
      return '(MASKED_URL_PATH)' + urlOrPattern.substr(urlPrefix.length);
    return urlOrPattern;
  }
})();
