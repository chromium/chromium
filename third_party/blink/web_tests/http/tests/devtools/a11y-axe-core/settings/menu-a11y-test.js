// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

(async function() {
  TestRunner.addResult(
      'Tests accessibility in the settings menu using the axe-core linter.');

  await TestRunner.loadLegacyModule('settings');

  await UI.actionRegistry.action('settings.show').execute();

  const tabbedPane = Settings.SettingsScreen.instance().tabbedLocation.tabbedPane();

  // force tabs to update
  tabbedPane.innerUpdateTabElements();

  await AxeCoreTestRunner.runValidation([tabbedPane.headerElement, tabbedPane.tabsElement]);
  TestRunner.completeTest();
})();
