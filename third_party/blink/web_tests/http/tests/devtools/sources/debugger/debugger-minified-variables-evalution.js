// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourceMapScopesModule from 'devtools/models/source_map_scopes/source_map_scopes.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

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

  function step3(uiSourceCode) {
    var positions = [
      new Position(7, 11, 23, 'object.prop1'), new Position(4, 4, 14, 'this.prop2'),
      new Position(5, 4, 19, 'object["prop3"]'), new Position(2, 8, 14, 'object'),  //object
    ];
    var promise = Promise.resolve();
    for (var position of positions)
      promise = promise.then(testAtPosition.bind(null, uiSourceCode, position));

    promise.then(() => SourcesTestRunner.completeDebuggerTest());
  }

  function Position(line, startColumn, endColumn, originText) {
    this.line = line;
    this.startColumn = startColumn;
    this.endColumn = endColumn;
    this.originText = originText;
  }

  function testAtPosition(uiSourceCode, position) {
    return SourceMapScopesModule.NamesResolver
        .resolveExpression(
            UIModule.Context.Context.instance().flavor(SDK.DebuggerModel.CallFrame), position.originText, uiSourceCode, position.line,
            position.startColumn, position.endColumn)
        .then(SourcesTestRunner.evaluateOnCurrentCallFrame)
        .then(result => TestRunner.addResult(result.object.description));
  }
})();
