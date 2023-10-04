// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that continue to location markers are computed correctly.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
    function testFunction() {
      eval(` + '`' + `debugger;
      foo1();
      Promise.resolve().then(foo2);
      Promise.resolve().then(() => 42);
      setTimeout(foo2);
      function foo1() {}
      function foo2() {}
      ` + '`' + `);
    }
    `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    TestRunner
        .addSnifferPromise(
            Sources.DebuggerPlugin.DebuggerPlugin.prototype,
            '_continueToLocationRenderedForTest')
        .then(step2);
    TestRunner.addSniffer(
        Sources.DebuggerPlugin.DebuggerPlugin.prototype, '_executionLineChanged', function() {
          SourcesTestRunner.showUISourceCodePromise(this._uiSourceCode)
              .then(() => {
                this._showContinueToLocations();
              });
        });
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
  }

  function step2() {
    var currentFrame = Sources.SourcesPanel.SourcesPanel.instance().visibleView;
    var debuggerPlugin = SourcesTestRunner.debuggerPlugin(currentFrame);
    var decorations = debuggerPlugin._continueToLocationDecorations;
    var lines = [];
    for (var decoration of decorations.keys()) {
      var find = decoration.find();
      var line = find.from.line;
      var text = currentFrame.textEditor.line(line).substring(find.from.ch, find.to.ch);
      lines.push(`${decoration.className} @${line + 1}:${find.from.ch} = '${text}'`);
    }
    lines.sort();
    TestRunner.addResults(lines);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
