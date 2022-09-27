// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests to ensure cache is disabled when interception is enabled.\n`);

  Common.moduleSetting('cacheDisabled').addChangeListener(cacheSettingChanged);

  TestRunner.addResult('Enabling Interception');
  await SDK.multitargetNetworkManager.setInterceptionHandlerForPatterns([{urlPattern: '*'}], () => Promise.resolve());
  TestRunner.addResult('Interception Enabled');
  TestRunner.completeTest();

  function cacheSettingChanged() {
    TestRunner.addResult('Cache Settings changed to: ' + Common.moduleSetting('cacheDisabled').get());
  }
})();
