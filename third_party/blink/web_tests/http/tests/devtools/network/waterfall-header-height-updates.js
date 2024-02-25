// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests to ensure network waterfall column updates header height when headers are not visible.\n`);
  await TestRunner.showPanel('network');
  await NetworkTestRunner.clearNetworkCache();

  NetworkTestRunner.recordNetwork();
  TestRunner.addResult('Setting initial large row setting to false');
  Network.NetworkPanel.NetworkPanel.instance().networkLogLargeRowsSetting.set(false);

  TestRunner.addResult('Fetching resource');
  await TestRunner.evaluateInPagePromise(`fetch('resources/empty.html?xhr')`);

  var request = await TestRunner.waitForEvent(
      SDK.NetworkManager.Events.RequestFinished, TestRunner.networkManager,
      request => request.name() === 'empty.html?xhr');
  var xhrNode = await NetworkTestRunner.waitForNetworkLogViewNodeForRequest(request);
  TestRunner.addResult('Node rendered showing fetch resource');
  Network.NetworkPanel.NetworkPanel.instance().onRequestSelected({data: request});
  Network.NetworkPanel.NetworkPanel.instance().showRequestPanel();
  // Wait for NetworkLogViewColumn.updateRowsSize to update the header height
  await new Promise(window.requestAnimationFrame);
  TestRunner.addResult('Height of waterfall header: ' + NetworkTestRunner.networkWaterfallColumn().headerHeight);

  TestRunner.addResult('Setting large row setting to true');
  Network.NetworkPanel.NetworkPanel.instance().networkLogLargeRowsSetting.set(true);
  TestRunner.addResult('Unselecting request from grid');
  Network.NetworkPanel.NetworkPanel.instance().hideRequestPanel();
  // Wait for NetworkLogViewColumn.updateRowsSize to update the header height
  await new Promise(window.requestAnimationFrame);
  TestRunner.addResult('Height of waterfall header: ' + NetworkTestRunner.networkWaterfallColumn().headerHeight);

  TestRunner.completeTest();
})();
