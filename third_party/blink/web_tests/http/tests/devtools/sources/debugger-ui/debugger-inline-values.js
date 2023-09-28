// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests inline values rendering in the sources panel.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          var a = { k: 1 };
          var b = [1, 2, 3, 4, 5];
          var c = new Array(100); c[10] = 1;
          a.k = 2;
          a.l = window;
          b[1]++;
          b[2] = document.body;
      }
  `);

  SourcesTestRunner.startDebuggerTest(runTestFunction);
  SourcesTestRunner.setQuiet(true);

  var stepCount = 0;

  function runTestFunction() {
    TestRunner.addSniffer(
        SourcesModule.DebuggerPlugin.DebuggerPlugin.prototype, 'executionLineChanged',
        onSetExecutionLocation);
    TestRunner.evaluateInPage('setTimeout(testFunction, 0)');
  }

  async function onSetExecutionLocation(liveLocation) {
    TestRunner.deprecatedRunAfterPendingDispatches(dumpAndContinue.bind(
        null, this.textEditor, (await liveLocation.uiLocation()).lineNumber));
  }

  function dumpAndContinue(textEditor, lineNumber) {
    TestRunner.addResult('=========== 11< ==========');
    for (var i = 11; i < 21; ++i) {
      var output = ['[' + (i < 10 ? ' ' : '') + i + ']'];
      output.push(i == lineNumber ? '>' : ' ');
      output.push(textEditor.line(i));
      output.push('\t');
      textEditor.decorations.get(i).forEach(decoration => output.push(decoration.element.deepTextContent()));
      TestRunner.addResult(output.join(' '));
    }

    TestRunner.addSniffer(
        SourcesModule.DebuggerPlugin.DebuggerPlugin.prototype, 'executionLineChanged',
        onSetExecutionLocation);
    if (++stepCount < 10)
      SourcesTestRunner.stepOver();
    else
      SourcesTestRunner.completeDebuggerTest();
  }
})();
