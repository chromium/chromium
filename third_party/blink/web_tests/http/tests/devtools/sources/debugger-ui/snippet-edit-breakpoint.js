// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that breakpoints can be edited in snippets before execution.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  const uiSourceCode1 = await Snippets.project.createFile('s1', null, '');
  uiSourceCode1.setContent('var x = 0;\n');
  TestRunner.addResult('Snippet content:');
  TestRunner.addResult((await uiSourceCode1.requestContent()).content);

  let sourceFrame = await SourcesTestRunner.showScriptSourcePromise("Script%20snippet%20%231");
  await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);

  SourcesTestRunner.toggleBreakpoint(sourceFrame, 0, true);
  await SourcesTestRunner.waitDebuggerPluginDecorations();
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  SourcesTestRunner.toggleBreakpoint(sourceFrame, 0, true);
  await SourcesTestRunner.waitDebuggerPluginDecorations();
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.completeTest();
})();
