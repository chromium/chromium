// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Ensures the icon is properly displayed when network request blocking setting is enabled/disabled.\n`);

  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(false);
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([]);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(true);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([{url: '*', enabled: true}]);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(false);
  dumpIconResult();

  TestRunner.addResult('Loading Network Module');
  await import('devtools/panels/network/network.js');
  TestRunner.addResult('Network Module Loaded');

  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(false);
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([]);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(true);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns([{url: '*', enabled: true}]);
  dumpIconResult();
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(false);
  dumpIconResult();
  TestRunner.completeTest();

  function dumpIconResult() {
    const icons = UI.InspectorView.InspectorView.instance().tabbedPane.tabsElement.getElementsByTagName('devtools-icon');
    const warnings = [...icons].filter(icon => icon.data.iconName === 'warning-filled');
    TestRunner.addResult('Is blocking: ' + SDK.NetworkManager.MultitargetNetworkManager.instance().isBlocking());
    TestRunner.addResult(Boolean(warnings.length) ? 'Has Icon' : 'Does Not Have Icon');
    TestRunner.addResult('');
  }
})();
