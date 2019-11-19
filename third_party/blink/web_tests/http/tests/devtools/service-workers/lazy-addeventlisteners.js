// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function wait_for_message(number_of_messages) {
  return new Promise(resolve => {
    ConsoleTestRunner.waitForConsoleMessages(number_of_messages, () => {
      resolve();
    });
  });
}

(async function() {
  TestRunner.addResult(
      `Tests that a warning is shown in the console if addEventListener is called after initial evaluation of the service worker script.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('resources');

  let scriptURL;
  let scope;

  // Test a lifecycle event.
  scriptURL =
    'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-lazy-install-addeventlistener.js';
  scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/lazy-install-scope';
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await wait_for_message(/*number_of_messages=*/1);

  // Test a functional event.
  scriptURL =
    'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-lazy-fetch-addeventlistener.js';
  scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/lazy-fetch-scope';
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await wait_for_message(/*number_of_messages=*/2);

  ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
