// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {

  var DBNAME = 'multiEntry-crash-test';
  var request = indexedDB.deleteDatabase(DBNAME);
  request.onsuccess = function (e) {
    request = indexedDB.open(DBNAME, 1);
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function (e) {
      var db = e.target.result;
      var store = db.createObjectStore('storeName');
      window.index1 = store.createIndex('index1Name', 'prop1');
      window.index2 = store.createIndex(
        'index2Name', 'prop2', {multiEntry: true});
      shouldBeFalse("window.index1.multiEntry");
      shouldBeTrue("window.index2.multiEntry");
      done();
    };
  };
}
