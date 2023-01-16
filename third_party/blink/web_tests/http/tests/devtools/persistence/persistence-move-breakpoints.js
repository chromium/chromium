// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test is testing the old breakpoint sidebar pane. Make sure to
  // turn off the new breakpoint pane experiment.
  Root.Runtime.experiments.setEnabled('breakpointView', false);
  TestRunner.addResult(`Verify that breakpoints are moved appropriately\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');
  await TestRunner.showPanel('sources');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    async function setBreakpointInFileSystemUISourceCode(next) {
      var uiSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
      var sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
      await SourcesTestRunner.setBreakpoint(sourceFrame, 2, '', true);
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
    var pane = Sources.JavaScriptBreakpointsSidebarPane.instance();
    if (!pane.emptyElement.classList.contains('hidden'))
      return TestRunner.textContentWithLineBreaks(pane.emptyElement);
    var entries = Array.from(pane.contentElement.querySelectorAll('.breakpoint-entry'));
    for (var entry of entries) {
      var uiLocation = Sources.JavaScriptBreakpointsSidebarPane.retrieveLocationForElement(entry);
      TestRunner.addResult('    ' + uiLocation.uiSourceCode.url() + ':' + uiLocation.lineNumber);
    }
  }
})();
