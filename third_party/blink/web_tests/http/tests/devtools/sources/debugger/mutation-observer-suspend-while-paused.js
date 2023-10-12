// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that DOM Mutation Observers do not attempt to deliver mutation records while the debugger is paused.Bug 105810\n`);
  await TestRunner.showPanel('sources');

  var setup = 'window.testDiv = document.createElement(\'div\');\n' +
      'window.deliveryCount = 0;\n' +
      'var observer = new WebKitMutationObserver(function(records) {\n' +
      '    window.deliveryCount++;\n' +
      '});\n' +
      'observer.observe(window.testDiv, { attributes: true });';

  var mutateAndPause = 'function mutateAndPause() {\n' +
      '    window.testDiv.setAttribute(\'foo\', \'baz\');\n' +
      '    debugger;\n' +
      '};\n' +
      'setTimeout(mutateAndPause, 0);';

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.evaluateInPage(setup, function() {
      TestRunner.addResult('DIV and observer setup.');
      ConsoleTestRunner.evaluateInConsoleAndDump('deliveryCount', step2);
    });
  }

  function step2() {
    TestRunner.evaluateInPage('window.testDiv.setAttribute(\'foo\', \'bar\')', function() {
      TestRunner.addResult('setAttribute should have triggered delivery.');
      ConsoleTestRunner.evaluateInConsoleAndDump('deliveryCount', step3);
    });
  }

  function step3() {
    TestRunner.evaluateInPage(mutateAndPause, TestRunner.addResult.bind(TestRunner, 'mutateAndPause invoked.'));
    SourcesTestRunner.waitUntilPaused(step4);
  }

  function step4() {
    TestRunner.addResult('Delivery should not have taken place.');
    ConsoleTestRunner.evaluateInConsoleAndDump('deliveryCount', function() {
      SourcesTestRunner.resumeExecution(step5);
    });
  }

  function step5() {
    TestRunner.addResult('Second delivery should now have happened.');
    ConsoleTestRunner.evaluateInConsoleAndDump('deliveryCount', SourcesTestRunner.completeDebuggerTest);
  }
})();
