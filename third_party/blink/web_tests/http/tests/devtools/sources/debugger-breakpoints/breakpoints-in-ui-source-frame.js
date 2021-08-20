// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests setting breakpoint in source frame UI.');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/a.js');

  TestRunner.runTestSuite([
    async function setBreakpoint(next) {
      const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
      TestRunner.addResult('Set breakpoint');
      SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false);

      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      TestRunner.addResult('Disable breakpoint');
      SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, true);
      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      TestRunner.addResult('Delete breakpoint');
      SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false);
      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      next();
    },

    async function setDisabledBreakpoint(next) {
      const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');

      TestRunner.addResult('Set disabled breakpoint');
      SourcesTestRunner.createNewBreakpoint(sourceFrame, 9, '', false);

      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      TestRunner.addResult('Enable breakpoint');
      SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, true);
      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      TestRunner.addResult('Delete breakpoint');
      SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false);
      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      next();
    },

    async function setConditionalBreakpoint(next) {
      const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');

      TestRunner.addResult('Set conditional breakpoint');
      SourcesTestRunner.createNewBreakpoint(sourceFrame, 9, 'condition', true);

      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      TestRunner.addResult('Change a condition');
      const lineDecorations = SourcesTestRunner.debuggerPlugin(sourceFrame)
                                  ._lineBreakpointDecorations(9);
      lineDecorations[0].breakpoint.setCondition('');

      TestRunner.addResult('Dump breakpoints');
      await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
      SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

      next();
    }
  ]);
})();
