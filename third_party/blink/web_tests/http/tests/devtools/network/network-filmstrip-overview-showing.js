// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Network from 'devtools/panels/network/network.js';

(async function() {
  TestRunner.addResult(
      `Tests to make sure film strip and overview pane show if the other does not exist. http://crbug.com/723659\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  var networkPanel = Network.NetworkPanel.NetworkPanel.instance();
  var showOverviewSetting = Common.Settings.Settings.instance().createSetting('network-log-show-overview', true);
  var showFilmstripSetting = Common.Settings.Settings.instance().createSetting('network-record-film-strip-setting', false);

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
