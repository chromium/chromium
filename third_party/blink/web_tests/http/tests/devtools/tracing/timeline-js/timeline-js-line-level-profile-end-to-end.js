// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a line-level CPU profile is collected and shown in the text editor.`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.loadLegacyModule('source_frame');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('sources');

  await TestRunner.evaluateInPagePromise(`
      function performActions() {
        const endTime = Date.now() + 100;
        let s = 0;
        while (Date.now() < endTime) s += Math.cos(s);
        return s;
      }
      //# sourceURL=test_file.js`);

  let hasLineLevelInfo;
  do {
    await PerformanceTestRunner.evaluateWithTimeline('performActions()');

    const events = PerformanceTestRunner.timelineModel().inspectedTargetEvents();
    hasLineLevelInfo = events.some(e => e.name === 'ProfileChunk' && e.args.data.lines);
  } while (!hasLineLevelInfo);

  TestRunner.addSniffer(SourceFrame.SourcesTextEditor.prototype, 'setGutterDecoration', decorationAdded, true);
  SourcesTestRunner.showScriptSource('test_file.js', () => {});

  function decorationAdded(line, type, element) {
    if (type !== 'CodeMirror-gutter-performance' || line !== 16)
      return;
    const value = parseFloat(element.textContent);
    TestRunner.addResult(`Decoration found: ${isFinite(value)}`);
    TestRunner.completeTest();
  }
})();
