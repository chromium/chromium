// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that mobile, network, and CPU throttling interact with each other logically.\n`);
  await TestRunner.showPanel("network");
  await TestRunner.showPanel("timeline");
  await TestRunner.loadLegacyModule('emulation');
  await TestRunner.loadLegacyModule('mobile_throttling');
  await TestRunner.loadLegacyModule('network');
  await UI.viewManager.showView("network.config");

  var deviceModeView = new Emulation.DeviceModeView();

  var deviceModeThrottling = deviceModeView.toolbar.throttlingConditionsItem;
  var networkPanelThrottling = UI.panels.network.throttlingSelectForTest();
  var networkConfigView = Network.NetworkConfigView.instance();
  var networkConditionsDrawerThrottlingSelector =
      networkConfigView.contentElement.querySelector('.network-config-throttling select.chrome-select');
  var performancePanelNetworkThrottling = UI.panels.timeline.networkThrottlingSelect;
  var performancePanelCPUThrottling = UI.panels.timeline.cpuThrottlingSelect;

  function dumpThrottlingState() {
    TestRunner.addResult('=== THROTTLING STATE ===');
    var {download, upload, latency} = SDK.multitargetNetworkManager.networkConditions();
    TestRunner.addResult(`Network throttling - download: ${Math.round(download)} upload: ${Math.round(upload)} latency: ${latency}`);
    TestRunner.addResult('CPU throttling rate: ' + SDK.CPUThrottlingManager.instance().cpuThrottlingRate());
    TestRunner.addResult('Device mode throttling: ' + deviceModeThrottling.text);
    TestRunner.addResult('Network panel throttling: ' + networkPanelThrottling.selectedOption().text);
    TestRunner.addResult('Network conditions drawer throttling: ' + networkConditionsDrawerThrottlingSelector.value);
    TestRunner.addResult(
        'Performance panel network throttling: ' + performancePanelNetworkThrottling.selectedOption().text);
    TestRunner.addResult('Performance panel CPU throttling: ' + performancePanelCPUThrottling.selectedOption().text);
    TestRunner.addResult('========================\n');
  }

  TestRunner.addResult('Initial throttling state');
  dumpThrottlingState();

  TestRunner.addResult('Change to offline in device mode');
  SDK.multitargetNetworkManager.setNetworkConditions(MobileThrottling.OfflineConditions().network);
  MobileThrottling.throttlingManager().setCPUThrottlingRate(MobileThrottling.OfflineConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change to low-end mobile in device mode');
  SDK.multitargetNetworkManager.setNetworkConditions(MobileThrottling.LowEndMobileConditions().network);
  MobileThrottling.throttlingManager().setCPUThrottlingRate(MobileThrottling.LowEndMobileConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change network to Fast 3G');
  SDK.multitargetNetworkManager.setNetworkConditions(SDK.NetworkManager.Fast3GConditions);
  dumpThrottlingState();

  TestRunner.addResult('Change to mid-tier mobile in device mode');
  SDK.multitargetNetworkManager.setNetworkConditions(MobileThrottling.MidTierMobileConditions().network);
  MobileThrottling.throttlingManager().setCPUThrottlingRate(MobileThrottling.MidTierMobileConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change CPU throttling to low-end mobile');
  MobileThrottling.throttlingManager().setCPUThrottlingRate(SDK.CPUThrottlingManager.CPUThrottlingRates.LowEndMobile);
  dumpThrottlingState();

  TestRunner.addResult('Change CPU throttling to mid-tier mobile');
  MobileThrottling.throttlingManager().setCPUThrottlingRate(SDK.CPUThrottlingManager.CPUThrottlingRates.MidTierMobile);
  dumpThrottlingState();

  TestRunner.addResult('Change to no throttling in device mode');
  SDK.multitargetNetworkManager.setNetworkConditions(MobileThrottling.NoThrottlingConditions().network);
  MobileThrottling.throttlingManager().setCPUThrottlingRate(MobileThrottling.NoThrottlingConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.completeTest();
})();
