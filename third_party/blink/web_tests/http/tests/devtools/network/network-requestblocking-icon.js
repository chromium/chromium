// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `Ensures the icon is properly displayed when network request blocking setting is enabled/disabled.\n`);

  SDK.multitargetNetworkManager.setBlockingEnabled(false);
  SDK.multitargetNetworkManager.setBlockedPatterns([]);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockingEnabled(true);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockedPatterns([{url: '*', enabled: true}]);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockingEnabled(false);
  dumpIconResult();

  TestRunner.addResult('Loading Network Module');
  await TestRunner.loadLegacyModule('network');
  TestRunner.addResult('Network Module Loaded');

  SDK.multitargetNetworkManager.setBlockingEnabled(false);
  SDK.multitargetNetworkManager.setBlockedPatterns([]);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockingEnabled(true);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockedPatterns([{url: '*', enabled: true}]);
  dumpIconResult();
  SDK.multitargetNetworkManager.setBlockingEnabled(false);
  dumpIconResult();
  TestRunner.completeTest();

  function dumpIconResult() {
    const icons = UI.inspectorView.tabbedPane.tabsElement.getElementsByTagName('devtools-icon');
    const warnings = [...icons].filter(icon => icon.data.iconName === 'warning-filled');
    TestRunner.addResult('Is blocking: ' + SDK.multitargetNetworkManager.isBlocking());
    TestRunner.addResult(Boolean(warnings.length) ? 'Has Icon' : 'Does Not Have Icon');
    TestRunner.addResult('');
  }
})();
