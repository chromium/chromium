// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

function wait_for_message(number_of_messages) {
  return new Promise(resolve => {
    ConsoleTestRunner.waitForConsoleMessages(number_of_messages, () => {
      resolve();
    });
  });
}

(async function() {
  TestRunner.addResult(
      `Tests that a warning is shown in the console if addEventListener adds an empty fetch handler.\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('resources');

  const scriptURL =
    'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty-fetch-handler.js';
  const scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/empty-fetch-handler-scope';
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);

  // Expecting that a warning message on adding a no-op fetch handler is shown
  // to console, and captured here. It is compared with contents in
  // add-empty-fetch-handler-expected.txt.
  await wait_for_message(/*number_of_messages=*/1);
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
