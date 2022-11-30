// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that fetch logging works when XMLHttpRequest Logging is Enabled and doesn't show logs when it is Disabled.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function requestHelper(method, url, callback)
      {
          console.log("sending a " + method + " request to " + url);
          // Delay invoking callback to let didFinishLoading() a chance to fire and log
          // console message.
          function delayCallback()
          {
              setTimeout(callback, 0);
          }
          makeFetch(url, {method: method}).then(delayCallback);
      }

      function makeRequests()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          step1();
          return promise;

          function step1()
          {
              // Page that exists.
              requestHelper("GET", "resources/xhr-exists.html", step2);
          }

          function step2()
          {
              // Page that doesn't exist.
              requestHelper("GET", "resources/xhr-does-not-exist.html", step3);
          }

          function step3()
          {
              // POST to a page.
              requestHelper("POST", "resources/post-target.cgi", step4);
          }

          function step4()
          {
              // (Failed) cross-origin request
              requestHelper("GET", "http://localhost:8000/devtools/resources/xhr-exists.html", callback);
          }
      }
  `);

  TestRunner.addResult('Making requests with monitoring ENABLED');
  Common.settingForTest('monitoringXHREnabled').set(true);
  await TestRunner.callFunctionInPageAsync('makeRequests');
  await ConsoleTestRunner.waitForPendingViewportUpdates();
  await ConsoleTestRunner.dumpConsoleMessages();
  Console.ConsoleView.clearConsole();

  TestRunner.addResult('Making requests with monitoring DISABLED');
  Common.settingForTest('monitoringXHREnabled').set(false);
  await TestRunner.callFunctionInPageAsync('makeRequests');
  await ConsoleTestRunner.waitForPendingViewportUpdates();
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
