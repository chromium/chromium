// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(
      `Tests that evaluating an expression with an exception in the console provide correct exception information.\n`);

  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('\
        function foo()\n\
        {\n\
            throw {a:239};\n\
        }\n\
        function boo()\n\
        {\n\
            foo();\n\
        }\n\
        boo();', afterEvaluate);

  async function afterEvaluate() {
    await ConsoleTestRunner.dumpConsoleMessages();
    var viewMessages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
    var uiMessage = viewMessages[viewMessages.length - 1];
    var message = uiMessage.consoleMessage();
    var stackTrace = message.stackTrace;

    if (stackTrace.callFrames.length < 3) {
      TestRunner.addResult('FAILED: Stack size too small');
    } else {
      for (var i = 0; i < 3; ++i) {
        var frame = stackTrace.callFrames[i];
        TestRunner.addResult('call frame:' + frame.functionName + ' at ' + frame.url + ':' + frame.lineNumber);
      }
    }

    TestRunner.completeTest();
  }
})();
