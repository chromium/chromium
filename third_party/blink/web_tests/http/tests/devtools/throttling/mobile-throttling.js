// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Emulation from 'devtools/panels/emulation/emulation.js';
import * as MobileThrottling from 'devtools/panels/mobile_throttling/mobile_throttling.js';
import * as Network from 'devtools/panels/network/network.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Timeline from 'devtools/panels/timeline/timeline.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that mobile, network, and CPU throttling interact with each other logically.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.showPanel('timeline');
  await UI.ViewManager.ViewManager.instance().showView('network.config');

  const deviceModeView = new Emulation.DeviceModeView.DeviceModeView();

  const deviceModeThrottling = deviceModeView.toolbar.throttlingConditionsItem;
  const networkPanelThrottling = Network.NetworkPanel.NetworkPanel.instance().throttlingSelectForTest();
  const networkConfigView = Network.NetworkConfigView.NetworkConfigView.instance();
  const networkConditionsDrawerThrottlingSelector =
      networkConfigView.contentElement.querySelector('.network-config-throttling select.chrome-select');
  const performancePanelNetworkThrottling = Timeline.TimelinePanel.TimelinePanel.instance().networkThrottlingSelect;
  const performancePanelCPUThrottling = Timeline.TimelinePanel.TimelinePanel.instance().cpuThrottlingSelect;

  function dumpThrottlingState() {
    TestRunner.addResult('=== THROTTLING STATE ===');
    const {download, upload, latency} = SDK.NetworkManager.MultitargetNetworkManager.instance().networkConditions();
    TestRunner.addResult(`Network throttling - download: ${Math.round(download)} upload: ${Math.round(upload)} latency: ${latency}`);
    TestRunner.addResult('CPU throttling rate: ' + SDK.CPUThrottlingManager.CPUThrottlingManager.instance().cpuThrottlingRate());
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
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getOfflineConditions().network);
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getOfflineConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change to low-end mobile in device mode');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getLowEndMobileConditions().network);
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getLowEndMobileConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change network to 3G');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(SDK.NetworkManager.Slow3GConditions);
  dumpThrottlingState();

  TestRunner.addResult('Change network to slow 4G');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(SDK.NetworkManager.Slow4GConditions);
  dumpThrottlingState();

  TestRunner.addResult('Change network to fast 4G');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(SDK.NetworkManager.Fast4GConditions);
  dumpThrottlingState();

  TestRunner.addResult('Change to mid-tier mobile in device mode');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getMidTierMobileConditions().network);
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getMidTierMobileConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.addResult('Change CPU throttling to low-end mobile');
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(SDK.CPUThrottlingManager.CPUThrottlingRates.LowEndMobile);
  dumpThrottlingState();

  TestRunner.addResult('Change CPU throttling to mid-tier mobile');
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(SDK.CPUThrottlingManager.CPUThrottlingRates.MidTierMobile);
  dumpThrottlingState();

  TestRunner.addResult('Change to no throttling in device mode');
  SDK.NetworkManager.MultitargetNetworkManager.instance().setNetworkConditions(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getNoThrottlingConditions().network);
  MobileThrottling.ThrottlingManager.throttlingManager().setCPUThrottlingRate(MobileThrottling.ThrottlingPresets.ThrottlingPresets.getNoThrottlingConditions().cpuThrottlingRate);
  dumpThrottlingState();

  TestRunner.completeTest();
})();
