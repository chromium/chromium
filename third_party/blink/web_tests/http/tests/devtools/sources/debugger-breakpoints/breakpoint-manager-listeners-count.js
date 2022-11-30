// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that scripts panel does not create too many source frames.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/breakpoint-manager-listeners-count.html');

  SourcesTestRunner.runDebuggerTestSuite([function testSourceFramesCount(next) {
    SourcesTestRunner.showScriptSource('script1.js', didShowScriptSources);

    function didShowScriptSources() {
      TestRunner.reloadPage(didReload);
    }

    function didReload() {
      SourcesTestRunner.showScriptSource(
          'script2.js', didShowScriptSourceAgain);
    }

    function didShowScriptSourceAgain() {
      var listeners = Bindings.breakpointManager.listeners.get(
          Bindings.BreakpointManager.Events.BreakpointAdded);
      // There should be 3 breakpoint-added event listeners:
      //  - BreakpointsSidebarPane
      //  - 2 shown tabs
      TestRunner.addResult(
          'Number of breakpoint-added event listeners is ' + listeners.size);

      function dumpListener(listener) {
        if (!(listener.thisObject instanceof Sources.DebuggerPlugin))
          return;
        var sourceFrame = listener.thisObject;
        TestRunner.addResult('    ' + sourceFrame.uiSourceCode.name());
      }

      TestRunner.addResult(
          'Dumping SourceFrames listening for breakpoint-added event:');
      [...listeners].map(dumpListener);

      next();
    }
  }]);
})();
