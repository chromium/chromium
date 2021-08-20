// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that breakpoints survive external editing.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');
  await TestRunner.showPanel('sources');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    async function setBreakpointInFileSystemUISourceCode(next) {
      testMapping.addBinding('foo.js');
      await BindingsTestRunner.waitForBinding('foo.js');
      var uiSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
      var sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
      await SourcesTestRunner.setBreakpoint(sourceFrame, 2, '', true);
      const debuggerPlugin = SourcesTestRunner.debuggerPlugin(sourceFrame);
      await TestRunner.addSnifferPromise(
          debuggerPlugin, '_breakpointDecorationsUpdatedForTest');
      await SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
      next();
    },

    async function addCommitToFilesystemUISourceCode(next) {
      await new Promise(x => setTimeout(x, 1000));
      var uiSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
      var sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
      var promise = TestRunner.addSnifferPromise(SDK.DebuggerModel.prototype, '_didEditScriptSource');
      uiSourceCode.addRevision(`
var w = 'some content';
var x = 'new content'; var inline = 'something else';
var y = 'more new content';`);
      await promise;
      const debuggerPlugin = SourcesTestRunner.debuggerPlugin(sourceFrame);
      await TestRunner.addSnifferPromise(
          debuggerPlugin, '_breakpointDecorationsUpdatedForTest');
      await SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
      next();
    }
  ]);
})();