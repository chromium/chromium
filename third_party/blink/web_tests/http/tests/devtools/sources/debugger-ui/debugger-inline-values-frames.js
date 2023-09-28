// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests inline values rendering while stepping between call frames.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          var sameName = 'foo';
          innerFunction('not-foo');
      }

      function innerFunction(sameName) {
        return;
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
    var startLine = 11;
    var endLine = 19;
    TestRunner.addResult(`=========== ${startLine}< ==========`);
    var lineCount = endLine - startLine;
    for (var i = startLine; i < endLine; ++i) {
      var output = ['[' + (i < lineCount ? ' ' : '') + i + ']'];
      output.push(i == lineNumber ? '>' : ' ');
      output.push(textEditor.line(i));
      output.push('\t');
      textEditor.decorations.get(i).forEach(decoration => output.push(decoration.element.deepTextContent()));
      TestRunner.addResult(output.join(' '));
    }

    TestRunner.addSniffer(
        SourcesModule.DebuggerPlugin.DebuggerPlugin.prototype, 'executionLineChanged',
        onSetExecutionLocation);
    if (++stepCount < 6)
      SourcesTestRunner.stepInto();
    else
      SourcesTestRunner.completeDebuggerTest();
  }
})();
