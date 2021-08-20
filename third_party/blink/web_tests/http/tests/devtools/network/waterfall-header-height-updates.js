// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests to ensure network waterfall column updates header height when headers are not visible.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');
  await NetworkTestRunner.clearNetworkCache();

  NetworkTestRunner.recordNetwork();
  TestRunner.addResult('Setting initial large row setting to false');
  UI.panels.network._networkLogLargeRowsSetting.set(false);

  TestRunner.addResult('Fetching resource');
  await TestRunner.evaluateInPagePromise(`fetch('resources/empty.html?xhr')`);

  var request = await TestRunner.waitForEvent(
      SDK.NetworkManager.Events.RequestFinished, TestRunner.networkManager,
      request => request.name() === 'empty.html?xhr');
  var xhrNode = await NetworkTestRunner.waitForNetworkLogViewNodeForRequest(request);
  TestRunner.addResult('Node rendered showing fetch resource');
  UI.panels.network._onRequestSelected({data: request});
  UI.panels.network._showRequestPanel();
  // Wait for NetworkLogViewColumn._updateRowsSize to update the header height
  await new Promise(window.requestAnimationFrame);
  TestRunner.addResult('Height of waterfall header: ' + NetworkTestRunner.networkWaterfallColumn()._headerHeight);

  TestRunner.addResult('Setting large row setting to true');
  UI.panels.network._networkLogLargeRowsSetting.set(true);
  TestRunner.addResult('Unselecting request from grid');
  UI.panels.network._hideRequestPanel();
  // Wait for NetworkLogViewColumn._updateRowsSize to update the header height
  await new Promise(window.requestAnimationFrame);
  TestRunner.addResult('Height of waterfall header: ' + NetworkTestRunner.networkWaterfallColumn()._headerHeight);

  TestRunner.completeTest();
})();
