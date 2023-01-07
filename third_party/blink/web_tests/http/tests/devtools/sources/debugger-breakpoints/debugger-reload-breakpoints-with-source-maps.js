// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests "reload" from within inspector window while on pause.`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/debugger-reload-breakpoints-with-source-maps.html');
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.showScriptSource('source1.js', step2);
  }

  async function step2(sourceFrame) {
    SourcesTestRunner.waitBreakpointSidebarPane()
        .then(waitUntilReady)
        .then(onBreakpointsReady);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 14, '', true);

    function onBreakpointsReady() {
      SourcesTestRunner.dumpBreakpointSidebarPane('before reload:');
      Promise
          .all([
            SourcesTestRunner.waitBreakpointSidebarPane().then(waitUntilReady),
            new Promise(resolve => TestRunner.reloadPage(resolve))
          ])
          .then(finishIfReady);
    }

    function finishIfReady() {
      SourcesTestRunner.dumpBreakpointSidebarPane('after reload:');
      SourcesTestRunner.completeDebuggerTest();
    }
  }

  function waitUntilReady() {
    var expectedBreakpointLocations = [[16, 4]];
    var paneElement =
        Sources.JavaScriptBreakpointsSidebarPane.instance().contentElement;
    var entries = Array.from(paneElement.querySelectorAll('.breakpoint-entry'));
    for (var entry of entries) {
      var uiLocation = Sources.JavaScriptBreakpointsSidebarPane.retrieveLocationForElement(entry);
      if (Bindings.CompilerScriptMapping.StubProjectID ===
          uiLocation.uiSourceCode.project().id())
        return SourcesTestRunner.waitBreakpointSidebarPane().then(
            waitUntilReady);
      if (!uiLocation.uiSourceCode.url().endsWith('source1.js'))
        return SourcesTestRunner.waitBreakpointSidebarPane().then(
            waitUntilReady);
      expectedBreakpointLocations = expectedBreakpointLocations.filter(
          (location) =>
              (location[0] != uiLocation.lineNumber &&
               location[1] != uiLocation.columnNumber));
    }
    if (expectedBreakpointLocations.length)
      return SourcesTestRunner.waitBreakpointSidebarPane().then(waitUntilReady);
    return Promise.resolve();
  }
})();
