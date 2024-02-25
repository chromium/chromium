// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests traceCalls(fn) console command.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function simpleTestFunction()
      {
         return 0;
      }
    `);
  await TestRunner.evaluateInPagePromise(`
      function simpleTestFunction2()
      {
         return simpleTestFunction3();
      }

      function simpleTestFunction3()
      {
         return 0;
      }
    `);

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testSimpleMonitor(next) {
      monitorAndRun(next, 'simpleTestFunction', 'simpleTestFunction();');
    },

    function testSimpleMonitorWith1Arg(next) {
      monitorAndRun(next, 'simpleTestFunction', 'simpleTestFunction(1);');
    },

    function testSimpleMonitorWithManyArgs(next) {
      monitorAndRun(next, 'simpleTestFunction', 'simpleTestFunction(1, 2, 3, 4 ,5);');
    },

    function testSimpleUnmonitor(next) {
      ConsoleTestRunner.evaluateInConsole('monitor(simpleTestFunction2)');
      ConsoleTestRunner.evaluateInConsole('unmonitor(simpleTestFunction2)');
      monitorAndRun(next, 'simpleTestFunction3', 'simpleTestFunction2();');
    },

    function testUnmonitorFuntionNotMonitored(next) {
      ConsoleTestRunner.evaluateInConsole('monitor(simpleTestFunction)', next);
    }
  ]);

  function monitorAndRun(next, functionName, runCmd) {
    ConsoleTestRunner.evaluateInConsole('monitor(' + functionName + ')');
    TestRunner.addResult('Start monitoring function.');

    ConsoleTestRunner.evaluateInConsole('setTimeout(function() { ' + runCmd + ' }, 0)');
    TestRunner.addResult('Set timer for test function.');
    ConsoleTestRunner.waitUntilMessageReceived(didReceive);

    function didReceive(message) {
      if (message.type === SDK.ConsoleModel.FrontendMessageType.Result) {
        ConsoleTestRunner.waitUntilMessageReceived(didReceive);
        return;
      }

      TestRunner.addResult('Console message received: ' + message.messageText);
      ConsoleTestRunner.evaluateInConsole('unmonitor(' + functionName + ')');
      TestRunner.addResult('Stop monitoring.');
      next();
    }
  }
})();
