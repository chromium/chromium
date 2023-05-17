// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests to make sure film strip and overview pane show if the other does not exist. http://crbug.com/723659\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  var networkPanel = UI.panels.network;
  var showOverviewSetting = Common.settings.createSetting('networkLogShowOverview', true);
  var showFilmstripSetting = Common.settings.createSetting('networkRecordFilmStripSetting', false);

  TestRunner.addResult('Overview should not be showing');
  TestRunner.addResult('Filmstrip should not be showing');
  showOverviewSetting.set(false);
  showFilmstripSetting.set(false);

  TestRunner.addResult('Overview Showing: ' + isOverviewShowing());
  TestRunner.addResult('Filmstrip Showing: ' + isFilmstripShowing());
  TestRunner.addResult('');

  TestRunner.addResult('Overview should be showing');
  TestRunner.addResult('Filmstrip should not be showing');
  showOverviewSetting.set(true);
  showFilmstripSetting.set(false);

  TestRunner.addResult('Overview Showing: ' + isOverviewShowing());
  TestRunner.addResult('Filmstrip Showing: ' + isFilmstripShowing());
  TestRunner.addResult('');

  TestRunner.addResult('Overview should not be showing');
  TestRunner.addResult('Filmstrip should be showing');
  showOverviewSetting.set(false);
  showFilmstripSetting.set(true);

  TestRunner.addResult('Overview Showing: ' + isOverviewShowing());
  TestRunner.addResult('Filmstrip Showing: ' + isFilmstripShowing());
  TestRunner.addResult('');

  TestRunner.addResult('Overview should be showing');
  TestRunner.addResult('Filmstrip should be showing');
  showOverviewSetting.set(true);
  showFilmstripSetting.set(true);

  TestRunner.addResult('Overview Showing: ' + isOverviewShowing());
  TestRunner.addResult('Filmstrip Showing: ' + isFilmstripShowing());
  TestRunner.addResult('');

  TestRunner.completeTest();

  function isOverviewShowing() {
    if (!networkPanel.overviewPane)
      return false;
    return networkPanel.overviewPane.isShowing();
  }

  function isFilmstripShowing() {
    if (!networkPanel.filmStripView)
      return false;
    return networkPanel.filmStripView.isShowing();
  }
})();
