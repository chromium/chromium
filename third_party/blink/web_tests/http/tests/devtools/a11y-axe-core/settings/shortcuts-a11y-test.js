// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests accessibility in the settings tool shortcuts pane using the axe-core linter.');
  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadLegacyModule('settings');

  async function testShortcuts() {
    // Open a view that supports context menu action to open shortcuts panel
    await UI.viewManager.showView('sources');

    // Open Shortcuts pane using context menu action
    await UI.actionRegistry.action('settings.shortcuts').execute();

    const settingsPaneElement = Settings.SettingsScreen.instance().tabbedLocation.tabbedPane().contentElement;
    await AxeCoreTestRunner.runValidation(settingsPaneElement);
  }

  TestRunner.runAsyncTestSuite([testShortcuts]);
})();
