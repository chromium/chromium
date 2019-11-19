// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests list of user metrics performance codes and invocations.\n`);

  const testHistogramRecording = (panel, histogram, expectHistogram, testExecutor) => {
    let recordHistogramComplete;
    performance.mark = function(name) {
      TestRunner.addResult(`Performance mark: ${name}`);
      if (!expectHistogram)
        recordHistogramComplete();
    };
    InspectorFrontendHost.recordPerformanceHistogram = function(name, duration) {
      TestRunner.addResult(`Performance histogram: ${name} - positive duration?: ${duration > 0}`);
      recordHistogramComplete();
    };

    return new Promise((resolve, reject) => {
      Host.userMetrics._firedLaunchHistogram = undefined;
      Host.userMetrics.setLaunchPanel(panel);
      testExecutor(panel, histogram);
      recordHistogramComplete = resolve;
    });
  };

  const testPanelLoaded = (panel, histogram) =>
      testHistogramRecording(panel, histogram, true, Host.userMetrics.panelLoaded.bind(Host.userMetrics));

  const testShowView = (panel, histogram) =>
      testHistogramRecording(panel, histogram, false, UI.viewManager.showView.bind(UI.viewManager));

  TestRunner.addResult('recordPanelLoaded:');
  await testPanelLoaded('console', 'DevTools.Launch.Console');
  await testPanelLoaded('elements', 'DevTools.Launch.Elements');
  await testPanelLoaded('network', 'DevTools.Launch.Network');
  await testPanelLoaded('sources', 'DevTools.Launch.Sources');

  TestRunner.addResult('\nTest that loading the tool only triggers the marker:');
  await testShowView('console', 'DevTools.Launch.Console');
  await testShowView('elements', 'DevTools.Launch.Elements');
  await testShowView('network', 'DevTools.Launch.Network');
  await testShowView('sources', 'DevTools.Launch.Sources');

  TestRunner.completeTest();
})();
