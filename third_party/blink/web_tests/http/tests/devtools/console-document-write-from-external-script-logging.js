// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that ignored document.write() called from an external asynchronously loaded script is reported to console as a warning\n`);
  await TestRunner.evaluateInPagePromise(`
      function loadExternalScript()
      {
          var scriptElement = document.createElement("script");
          scriptElement.src = "resources/external-script-with-document-write.js";
          document.body.appendChild(scriptElement);
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step1);
  TestRunner.evaluateInPage('loadExternalScript()', function() {});

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
