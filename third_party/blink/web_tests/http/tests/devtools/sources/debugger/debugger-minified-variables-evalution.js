// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourceMapScopesModule from 'devtools/models/source_map_scopes/source_map_scopes.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Formatter from 'devtools/models/formatter/formatter.js';

(async function() {
  TestRunner.addResult(`Tests evaluation in minified scripts.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-expressions-compressed.js');

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest', step2)
  }

  function step2() {
    SourcesTestRunner.waitForScriptSource('resolve-expressions-origin.js', step3);
  }

  async function step3() {
    const expressions = ['object.prop1', 'this.prop2', 'object["prop3"]', 'object'];
    for (const expression of expressions) {
      await testWithExpression(expression);
    }

    SourcesTestRunner.completeDebuggerTest();
  }

  function testWithExpression(expression) {
    return SourceMapScopesModule.NamesResolver
      .allVariablesInCallFrame(UIModule.Context.Context.instance().flavor(SDK.DebuggerModel.CallFrame))
      .then(map => Formatter.FormatterWorkerPool.formatterWorkerPool().javaScriptSubstitute(expression, map))
      .then(SourcesTestRunner.evaluateOnCurrentCallFrame)
      .then(result => TestRunner.addResult(result.object.description));
  }
})();
