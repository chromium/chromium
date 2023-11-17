// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as Settings from 'devtools/panels/settings/settings.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the settings tool shortcuts pane using the axe-core linter.');

  async function testShortcuts() {
    // Open a view that supports context menu action to open shortcuts panel
    await UI.ViewManager.ViewManager.instance().showView('sources');

    // Open Shortcuts pane using context menu action
    await UI.ActionRegistry.ActionRegistry.instance().getAction('settings.shortcuts').execute();

    const settingsPaneElement = Settings.SettingsScreen.SettingsScreen.instance().tabbedLocation.tabbedPane().contentElement;
    await AxeCoreTestRunner.runValidation(settingsPaneElement);
  }

  TestRunner.runAsyncTestSuite([testShortcuts]);
})();
