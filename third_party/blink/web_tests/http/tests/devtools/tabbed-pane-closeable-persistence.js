// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests extensible tabbed pane closeable tabs persistence logic.\n`);


  var tabbedLocation = UI.viewManager.createTabbedLocation();
  logPersistenceSetting();

  // Show a closeable tab.
  var sensors = new UI.SimpleView('sensors');
  sensors.isCloseable = function() {
    return true;
  };
  tabbedLocation.showView(sensors);
  logPersistenceSetting();

  // Repeat.
  tabbedLocation.showView(sensors);
  logPersistenceSetting();

  // Show a permanent tab.
  var console = new UI.SimpleView('console');
  tabbedLocation.showView(console);
  logPersistenceSetting();

  // Show transient tab.
  var history = new UI.SimpleView('history');
  history.isTransient = function() {
    return true;
  };
  tabbedLocation.showView(history);
  logPersistenceSetting();

  // Close closeable tab.
  tabbedLocation.tabbedPane().closeTab('sensors');
  logPersistenceSetting();

  TestRunner.completeTest();

  function logPersistenceSetting() {
    TestRunner.addResult('Closeable tabs to restore: ' + JSON.stringify(tabbedLocation.getCloseableTabSetting()));
  }
})();
