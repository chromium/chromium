// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that main resource script text is correct when paused in inline script on reload.`);

  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.navigatePromise('resources/pause-in-inline-script.html');
  await SourcesTestRunner.waitUntilPausedPromise();

  TestRunner.addResult('Did load front-end');
  TestRunner.addResult(
      'Paused: ' + !!TestRunner.debuggerModel.debuggerPausedDetails());
  TestRunner.reloadPage(didReload.bind(this));
  TestRunner.debuggerModel.addEventListener(
      SDK.DebuggerModel.Events.DebuggerPaused, didPauseAfterReload, this);

  function didReload() {
    TestRunner.addResult('didReload');
    SourcesTestRunner.completeDebuggerTest();
  }

  function didPauseAfterReload(details) {
    TestRunner.addResult('didPauseAfterReload');
    TestRunner.addResult('Source strings corresponding to the call stack:');
    dumpNextCallFrame(didDump);
  }

  var callFrameIndex = 0;
  async function dumpNextCallFrame(next) {
    var callFrames = TestRunner.debuggerModel.callFrames;
    if (callFrameIndex === callFrames.length) {
      next();
      return;
    }
    var frame = callFrames[callFrameIndex];
    var uiLocation = await Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(
        frame.location());
    SourcesTestRunner.showUISourceCode(
        uiLocation.uiSourceCode, dumpCallFrameLine);

    function dumpCallFrameLine(sourceFrame) {
      var resourceText = sourceFrame._textEditor.text();
      var lines = resourceText.split('\n');
      var lineNumber = uiLocation.lineNumber;
      TestRunner.addResult(
          'Frame ' + callFrameIndex + ') line ' + lineNumber +
          ', content: ' + lines[lineNumber] + ' (must be part of function \'' +
          frame.functionName + '\')');
      callFrameIndex++;
      dumpNextCallFrame(next);
    }
  }

  function didDump() {
    SourcesTestRunner.resumeExecution(didResume);
  }

  function didResume() {
    TestRunner.addResult('didResume');
  }
})();
