// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that inline scope variables are rendering correctly.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var length = 123;
          var a = [1, 2, 3];
          if (a.length) {
              var b = 42;
              console.log(a.length);
              debugger;
              return b;
          }
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    TestRunner
        .addSnifferPromise(
            Sources.DebuggerPlugin.DebuggerPlugin.prototype, '_renderDecorations')
        .then(step2);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
  }

  function step2() {
    var currentFrame = Sources.SourcesPanel.SourcesPanel.instance().visibleView;
    var decorations = currentFrame.textEditor._decorations;
    for (var line of decorations.keysArray()) {
      var lineDecorations =
          Array.from(decorations.get(line)).map(decoration => decoration.element.textContent).join(', ');
      TestRunner.addResult(`${line + 1}: ${lineDecorations}`);
    }
    SourcesTestRunner.completeDebuggerTest();
  }
})();
