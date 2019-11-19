// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a line-level CPU profile is shown in the text editor.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../resources/empty.js');

  var cpuProfile = {
    startTime: 10e6,
    endTime: 20e6,
    nodes: [
      {id: 0, callFrame: {functionName: '(root)'}, hitCount: 0, children: [1, 2]}, {
        id: 1,
        callFrame: {functionName: 'foo1'},
        hitCount: 100,
        positionTicks: [{line: 1, ticks: 10}, {line: 2, ticks: 20}, {line: 3, ticks: 30}, {line: 4, ticks: 40}]
      },
      {
        id: 2,
        callFrame: {functionName: 'foo2'},
        hitCount: 200,
        positionTicks: [{line: 100, ticks: 1}, {line: 102, ticks: 190}],
        children: [3]
      },
      {id: 3, callFrame: {functionName: 'null'}, hitCount: 0, positionTicks: [], children: [4, 5]},
      {id: 4, callFrame: {functionName: 'bar'}, hitCount: 300, positionTicks: [{line: 55, ticks: 22}]}, {
        id: 5,
        callFrame: {functionName: 'baz'},
        hitCount: 400,
        // no positionTicks for the node.
        children: []
      }
    ]
  };

  TestRunner.addSniffer(SourceFrame.SourcesTextEditor.prototype, 'setGutterDecoration', decorationAdded, true);
  SourcesTestRunner.showScriptSource('empty.js', frameRevealed);

  function decorationAdded(line, type, element) {
    TestRunner.addResult(`${line} ${type} ${element.textContent} ${element.style.backgroundColor}`);
  }

  function frameRevealed(frame) {
    const url = frame.uiSourceCode().url();
    TestRunner.addResult(TestRunner.formatters.formatAsURL(url));
    cpuProfile.nodes.forEach(n => n.callFrame.url = url);
    const lineProfile = self.runtime.sharedInstance(PerfUI.LineLevelProfile.Performance);
    lineProfile.appendCPUProfile(new SDK.CPUProfileDataModel(cpuProfile));
    setTimeout(() => TestRunner.completeTest(), 0);
  }
})();
