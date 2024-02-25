// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Test that evaluation on call frame works across all inspected windows in the call stack.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <iframe id="iframe"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function dirxml() { return "overridden dirxml() in top frame"; }

      var iframe = document.getElementById("iframe");
      var doc = iframe.contentWindow.document;

      var html = "<html><head><script>\\n" +
          "function dir() { return 'overridden dir() in iframe'; }\\n" +
          "function pauseInsideIframe()\\n" +
          "{\\n" +
          "    var table = 'local in iframe';\\n" +
          "    debugger;\\n" +
          "    dir;\\n" +
          "}\\n" +
          "</" + "script></" + "head><" + "body></" + "body></" + "html>";
      doc.open();
      doc.write(html);
      doc.close();
      function testFunction()
      {
          var clear = "local in top frame";
          var iframe = document.getElementById("iframe");
          iframe.contentWindow.pauseInsideIframe.call({foo: 42});
      }
  `);

  var expressions = [
    'dir()', 'dirxml()', 'table', 'clear',
    'x:',  // print correct syntax error: crbug.com/110163
  ];

  function evaluateInConsoleAndDump(callback) {
    var copy = expressions.slice();
    inner();

    function inner() {
      var expression = copy.shift();
      if (expression)
        ConsoleTestRunner.evaluateInConsoleAndDump(expression, inner, true);
      else
        callback();
    }
  }

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  async function step2(callFrames) {
    await TestRunner.addSnifferPromise(SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest');
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('\n=== Evaluating on iframe ===');
    evaluateInConsoleAndDump(step3);
  }

  function step3() {
    var pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
    pane.selectNextCallFrameOnStack();
    TestRunner.deprecatedRunAfterPendingDispatches(step4);
  }

  function step4() {
    TestRunner.addResult('\n=== Evaluating on top frame ===');
    evaluateInConsoleAndDump(step5);
  }

  function step5() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
