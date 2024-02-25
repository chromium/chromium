// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that long URLs are correctly trimmed in anchor links.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadScript()
      {
          var i = document.createElement('script');
          i.src = "http://127.0.0.1:8000/inspector/network/resources/script-as-text-with-a-very-very-very-very-very-very-very-very-very-very-very-very-very-very-long-url.php";
          document.body.appendChild(i);
      }
  `);

  TestRunner.evaluateInPage('loadScript()');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
  await ConsoleTestRunner.dumpConsoleMessages(false, false, ConsoleTestRunner.prepareConsoleMessageTextTrimmed);
  TestRunner.completeTest();
})();
