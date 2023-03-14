// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a line-level CPU profile is collected and shown in the text editor.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadLegacyModule('source_frame');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('sources');

  await TestRunner.evaluateInPageAnonymously(`
      function performActions() {
        console.trace('Message to capture the scriptId');
        const endTime = Date.now() + 100;
        let s = 0;
        while (Date.now() < endTime) s += Math.cos(s);
        return s;
      }`);

  let scriptId;
  ConsoleTestRunner.addConsoleSniffer(m => {
    if (m.messageText === 'Message to capture the scriptId')
      scriptId = m.stackTrace.callFrames[0].scriptId;
  }, true);

  let hasLineLevelInfo;
  do {
    await PerformanceTestRunner.evaluateWithTimeline('performActions()');
    const events = PerformanceTestRunner.timelineModel().inspectedTargetEvents();
    hasLineLevelInfo = events.some(e => e.name === 'ProfileChunk' && e.args.data.lines);
  } while (!hasLineLevelInfo);

  TestRunner.addSniffer(SourceFrame.SourcesTextEditor.prototype, 'setGutterDecoration', decorationAdded, true);

  const debuggerModel = SDK.targetManager.primaryPageTarget().model(SDK.DebuggerModel);
  const rawLocation = debuggerModel.createRawLocationByScriptId(scriptId, 0, 0);
  const uiLocation = await Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(rawLocation);
  await SourcesTestRunner.showUISourceCodePromise(uiLocation.uiSourceCode);

  function decorationAdded(line, type, element) {
    if (type !== 'CodeMirror-gutter-performance' || line !== 5)
      return;
    const value = parseFloat(element.textContent);
    TestRunner.addResult(`Decoration found: ${isFinite(value)}`);
    TestRunner.completeTest();
  }
})();
