// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that webInspector.inspectedWindow.eval() only evaluates in the correct execution context\n`);
  // First navigate to a new page to force a nice, clean renderer with predictable context ids.
  await TestRunner.navigatePromise('http://devtools.a.test:8000/devtools/resources/empty.html');

  let pendingInterceptionPromiseCallback;
  TestRunner.startNavigation = async function(callback) {
    await SDK.NetworkManager.MultitargetNetworkManager.instance().setInterceptionHandlerForPatterns([{
      urlPattern: '*'}], interceptionHandler);

    TestRunner.navigatePromise('http://devtools.b.test:8000/devtools/resources/empty.html');
    function interceptionHandler(request) {
      callback();
      return new Promise(resolve => pendingInterceptionPromiseCallback = resolve);
    }
  }

  TestRunner.completeNavigation = function() {
    pendingInterceptionPromiseCallback();
  }

  await ExtensionsTestRunner.runExtensionTests([
    async function extension_testEvaluateInFixedExecutionContext(nextTest) {
      await evaluateOnFrontendPromise('TestRunner.startNavigation(reply)');
      webInspector.inspectedWindow.eval('location.href', onEvaluate);
      evaluateOnFrontendPromise('TestRunner.completeNavigation()');

      function onEvaluate(result, error) {
        if (result) {
          output(`FAIL: expected error, got result: ${JSON.stringify(result)}`);
        } else {
          output(`Got error, as expected: ${JSON.stringify(error)}`);;
        }
        nextTest();
      }
    }
  ]);
})();
