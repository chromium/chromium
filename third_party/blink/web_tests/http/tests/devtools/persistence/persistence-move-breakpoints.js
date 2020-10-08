// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that breakpoints are moved appropriately\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
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
      var uiSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
      var sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
      await SourcesTestRunner.setBreakpoint(sourceFrame, 0, '', true);
      await SourcesTestRunner.waitBreakpointSidebarPane();
      dumpBreakpointSidebarPane();
      next();
    },

    async function addFileMapping(next) {
      testMapping.addBinding('foo.js');
      await BindingsTestRunner.waitForBinding('foo.js');

      await SourcesTestRunner.waitBreakpointSidebarPane();
      dumpBreakpointSidebarPane();
      next();
    },

    function removeFileMapping(next) {
      Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
      testMapping.removeBinding('foo.js');

      async function onBindingRemoved(event) {
        var binding = event.data;
        if (binding.network.name() !== 'foo.js')
          return;
        Persistence.persistence.removeEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
        await SourcesTestRunner.waitBreakpointSidebarPane();
        dumpBreakpointSidebarPane();
        next();
      }
    },
  ]);

  function dumpBreakpointSidebarPane() {
    var pane = self.runtime.sharedInstance(Sources.JavaScriptBreakpointsSidebarPane);
    if (!pane._emptyElement.classList.contains('hidden'))
      return TestRunner.textContentWithLineBreaks(pane._emptyElement);
    var entries = Array.from(pane.contentElement.querySelectorAll('.breakpoint-entry'));
    for (var entry of entries) {
      var uiLocation = Sources.JavaScriptBreakpointsSidebarPane.retrieveLocationForElement(entry);
      TestRunner.addResult('    ' + uiLocation.uiSourceCode.url() + ':' + uiLocation.lineNumber);
    }
  }
})();
