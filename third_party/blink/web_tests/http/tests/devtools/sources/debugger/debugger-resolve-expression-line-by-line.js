// Copyright 2023 The Chromium Authors
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
  TestRunner.addResult(`Tests evaluation in webpack bundled scripts with 'line-by'line' source maps.\n`);
  await TestRunner.showPanel('sources');

  // Bundle created using `npx webpack` with 'cheap-module-source-map'.
  await TestRunner.addScriptTag('resources/resolve-expressions-webpack-bundle.js');

  await SourcesTestRunner.startDebuggerTestPromise();
  SourcesTestRunner.runTestFunctionAndWaitUntilPaused();

  await TestRunner.addSnifferPromise(SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest');
  SourcesTestRunner.waitForScriptSource('resolve-expressions-webpack-authored.js', async (uiSourceCode) => {
    const callFrame = UIModule.Context.Context.instance().flavor(SDK.DebuggerModel.CallFrame);
    const mappings = await SourceMapScopesModule.NamesResolver.allVariablesInCallFrame(callFrame);

    // "this.#prop" maps to the whole assignment, but we should be able to handle that..
    let resolvedExpression = await Formatter.FormatterWorkerPool.FormatterWorkerPool.instance().javaScriptSubstitute('this.#prop', mappings);
    TestRunner.addResult(`Resolved expression "this.#prop" to "${resolvedExpression}"`);

    // "a" should work as the "a++" is the only statement on the line.
    resolvedExpression = await Formatter.FormatterWorkerPool.FormatterWorkerPool.instance().javaScriptSubstitute('a', mappings);
    TestRunner.addResult(`Resolved expression "a" to "${resolvedExpression}"`);

    SourcesTestRunner.completeDebuggerTest();
  });
})();
