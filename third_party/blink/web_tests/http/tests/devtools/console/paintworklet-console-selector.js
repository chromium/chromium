// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests console execution context selector for paintworklet.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
    <script id="code" type="text/worklet">
      registerPaint('foo', class { paint() { } });
    </script>
  `);
  await TestRunner.evaluateInPagePromise(`
      function setup() {
        var blob = new Blob([code.textContent], {type: 'text/javascript'});
        return CSS.paintWorklet.addModule(URL.createObjectURL(blob));
      }
  `);

  await new Promise(f => SourcesTestRunner.startDebuggerTest(f, true));
  await TestRunner.evaluateInPageAsync('setup()');

  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var selector = consoleView.consoleContextSelector;
  TestRunner.addResult('Console context selector:');
  for (var executionContext of selector._items) {
    var selected = UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.RuntimeModel.ExecutionContext) === executionContext;
    var text = '____'.repeat(selector.depthFor(executionContext)) + selector.titleFor(executionContext) + " / " + selector._subtitleFor(executionContext);
    var disabled = !selector.isItemSelectable(executionContext);
    TestRunner.addResult(`${selected ? '*' : ' '} ${text} ${disabled ? '[disabled]' : ''}`);
  }

  SourcesTestRunner.completeDebuggerTest();
})();
