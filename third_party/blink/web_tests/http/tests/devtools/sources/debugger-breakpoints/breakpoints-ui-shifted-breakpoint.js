// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests shifted breakpoint.');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/a.js');

  let sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  TestRunner.addResult('Set enabled breakpoint close to shifted location');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 25, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 29, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Set disabled breakpoint close to shifted location');
  SourcesTestRunner.createNewBreakpoint(sourceFrame, 25, '', false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 25, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Set enabled breakpoint far away from shifted location');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 19, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 29, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Set disabled breakpoint far away from shifted location');
  SourcesTestRunner.createNewBreakpoint(sourceFrame, 19, '', false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 19, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Set breakpoint after script end');
  SourcesTestRunner.createNewBreakpoint(sourceFrame, 239, '', false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Set two breakpoints with the same shifted line');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 19, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 25, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 29, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.completeTest();
})();
