// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests if page keeps recording after refresh with Screenshot enabled. Bug 569557\n`);
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      <p id="test"></p>
    `);

  Network.NetworkPanel.NetworkPanel.instance().networkRecordFilmStripSetting.set(true);

  Network.NetworkPanel.NetworkPanel.displayScreenshotDelay = 0;

  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);
  TestRunner.runWhenPageLoads(() => setTimeout(checkRecording, 50));
  TestRunner.resourceTreeModel.reloadPage();

  function checkRecording() {
    TestRunner.addResult(Network.NetworkPanel.NetworkPanel.instance().networkLogView.recording ? 'Still recording' : 'Not recording');

    TestRunner.completeTest();
  }
})();
