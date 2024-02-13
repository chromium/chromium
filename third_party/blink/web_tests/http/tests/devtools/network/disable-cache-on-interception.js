// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests to ensure cache is disabled when interception is enabled.\n`);

  Common.Settings.moduleSetting('cache-disabled').addChangeListener(cacheSettingChanged);

  TestRunner.addResult('Enabling Interception');
  await SDK.NetworkManager.MultitargetNetworkManager.instance().setInterceptionHandlerForPatterns([{urlPattern: '*'}], () => Promise.resolve());
  TestRunner.addResult('Interception Enabled');
  TestRunner.completeTest();

  function cacheSettingChanged() {
    TestRunner.addResult('Cache Settings changed to: ' + Common.Settings.moduleSetting('cache-disabled').get());
  }
})();
