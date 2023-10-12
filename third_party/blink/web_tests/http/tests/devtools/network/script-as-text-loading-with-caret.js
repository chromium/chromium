// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests console message when script is loaded with incorrect text/html mime type and the URL contains the '^' character.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadScript()
      {
          var s = document.createElement("script");
          s.src = "resources/this-is-a-weird?querystring=with^carats^like^these^because^who^doesnt^love^strange^characters^in^urls";
          document.body.appendChild(s);
      }
  `);

  TestRunner.evaluateInPage('loadScript()');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
  await ConsoleTestRunner.dumpConsoleMessages(false, false, ConsoleTestRunner.prepareConsoleMessageTextTrimmed);
  TestRunner.completeTest();
})();
