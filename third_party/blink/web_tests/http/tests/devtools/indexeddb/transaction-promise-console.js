// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  'use strict';
  TestRunner.addResult(
      `Ensure transactions created within Promise callbacks are not deactivated due to console activity\n`);
  // Note: every test that uses a storage API must manually clean-up state from
  // previous tests.
  await ApplicationTestRunner.resetState();

  var dbname = location.href;
  indexedDB.deleteDatabase(dbname).onsuccess = function() {

    var openRequest = indexedDB.open(dbname);
    openRequest.onupgradeneeded = function() {
      openRequest.result.createObjectStore('store');
    };
    openRequest.onsuccess = function(event) {
      var db = event.target.result;
      Promise.resolve().then(function() {
        const tx = db.transaction('store');
        ConsoleTestRunner.evaluateInConsole('1 + 2');
        try {
          tx.objectStore('store').get(0);
          TestRunner.addResult('PASS: Transaction is still active');
        } catch (ex) {
          TestRunner.addResult('FAIL: ' + ex.message);
        } finally {
          TestRunner.completeTest();
        }
      });
    };
  };
})();
