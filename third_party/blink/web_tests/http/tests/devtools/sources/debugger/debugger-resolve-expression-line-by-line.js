// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests evaluation in webpack bundled scripts with 'line-by'line' source maps.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  // Bundle created using `npx webpack` with 'cheap-module-source-map'.
  await TestRunner.addScriptTag('resources/resolve-expressions-webpack-bundle.js');

  await SourcesTestRunner.startDebuggerTestPromise();
  SourcesTestRunner.runTestFunctionAndWaitUntilPaused();

  await TestRunner.addSnifferPromise(Sources.CallStackSidebarPane.prototype, 'updatedForTest');
  SourcesTestRunner.waitForScriptSource('resolve-expressions-webpack-authored.js', async (uiSourceCode) => {
    // "this.#prop" maps to the whole assignment so we should not resolve to anything in the source map.
    let resolvedExpression = await Sources.SourceMapNamesResolver.resolveExpression(UI.context.flavor(SDK.DebuggerModel.CallFrame), 'this.#prop', uiSourceCode, 5, 17, 27);
    TestRunner.addResult(`Resolved expression "this.#prop" to "${resolvedExpression}"`);

    // "a" should work as the "a++" is the only statement on the line.
    resolvedExpression = await Sources.SourceMapNamesResolver.resolveExpression(UI.context.flavor(SDK.DebuggerModel.CallFrame), 'a', uiSourceCode, 6, 4, 5);
    TestRunner.addResult(`Resolved expression "a" to "${resolvedExpression}"`);

    SourcesTestRunner.completeDebuggerTest();
  });
})();
