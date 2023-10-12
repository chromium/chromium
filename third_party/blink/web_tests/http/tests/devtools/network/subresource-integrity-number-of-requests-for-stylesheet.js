// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Verify that only one request is made for basic stylesheet requests with integrity attribute.\n`);
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
      // Regression test for https://crbug.com/573269.
      function loadIFrame() {
          var iframe = document.createElement('iframe');
          iframe.src = 'resources/style-with-integrity-frame.html';
          document.body.appendChild(iframe);
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step1);
  TestRunner.evaluateInPage('loadIFrame()');

  async function step1() {
    const requests = NetworkTestRunner.findRequestsByURLPattern(/style.css/)
                         .filter((e, i, a) => i % 2 == 0);
    TestRunner.assertTrue(requests.length === 1);
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
