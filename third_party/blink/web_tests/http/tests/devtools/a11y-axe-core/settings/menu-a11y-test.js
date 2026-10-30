// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as Settings from 'devtools/panels/settings/settings.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

// TODO(crbug.com/407751489): Remove the `tabbedLocation` fallback once the
// declarative SettingsScreen has rolled into Chromium.
function settingsTabbedPane() {
  const settingsScreen = Settings.SettingsScreen.SettingsScreen.instance();
  if (settingsScreen.tabbedLocation) {
    return settingsScreen.tabbedLocation.tabbedPane();
  }
  return UI.Widget.Widget.get(settingsScreen.contentElement.querySelector('devtools-tabbed-pane'));
}

(async function() {
  TestRunner.addResult(
      'Tests accessibility in the settings menu using the axe-core linter.');

  await UI.ActionRegistry.ActionRegistry.instance().getAction('settings.show').execute();

  const tabbedPane = settingsTabbedPane();

  // force tabs to update
  tabbedPane.performUpdate();

  await AxeCoreTestRunner.runValidation([tabbedPane.headerElement, tabbedPane.tabsElement]);
  TestRunner.completeTest();
})();
