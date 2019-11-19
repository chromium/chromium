// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Tests accessibility in the settings menu using the axe-core linter.');

  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('settings');

  await UI.actionRegistry.action('settings.show').execute();

  const tabbedPane = runtime.sharedInstance(Settings.SettingsScreen)
                         ._tabbedLocation.tabbedPane();

  // force tabs to update
  tabbedPane._innerUpdateTabElements();

  await AxeCoreTestRunner.runValidation(
      [tabbedPane._headerElement, tabbedPane._tabsElement]);
  TestRunner.completeTest();
})();
