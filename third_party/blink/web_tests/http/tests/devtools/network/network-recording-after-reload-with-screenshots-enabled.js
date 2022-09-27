// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests if page keeps recording after refresh with Screenshot enabled. Bug 569557\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      <p id="test"></p>
    `);

  UI.panels.network.networkRecordFilmStripSetting.set(true);

  Network.NetworkPanel.displayScreenshotDelay = 0;

  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);
  TestRunner.runWhenPageLoads(() => setTimeout(checkRecording, 50));
  TestRunner.resourceTreeModel.reloadPage();

  function checkRecording() {
    TestRunner.addResult(UI.panels.network.networkLogView.recording ? 'Still recording' : 'Not recording');

    TestRunner.completeTest();
  }
})();
