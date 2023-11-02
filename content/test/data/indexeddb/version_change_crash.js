// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  if (document.location.hash === '#part1') {
    testPart1();
  } else if (document.location.hash === '#part2') {
    testPart2();
  } else if (document.location.hash === '#part3') {
    testPart3();
  } else {
    result('fail - unexpected hash');
  }
}

function testPart1()
{
  // Prepare the database, then exit normally

  // Set version 1, create store1
  var delreq = window.indexedDB.deleteDatabase('version-change-crash');
  delreq.onerror = unexpectedErrorCallback;
  delreq.onsuccess = function() {
    var openreq = window.indexedDB.open('version-change-crash', 1);
    openreq.onerror = unexpectedErrorCallback;
    openreq.onblocked = unexpectedBlockedCallback;
    openreq.onupgradeneeded = function(e) {
      openreq.result.createObjectStore('store1');
    };
    openreq.onsuccess = function(e) {
      result('pass - part1 - complete');
    };
  };
}

function testPart2()
{
  // Start a VERSION_CHANGE then crash

  // Set version 2, twiddle stores and crash
  var openreq = window.indexedDB.open('version-change-crash', 2);
  openreq.onerror = unexpectedErrorCallback;
  openreq.onblocked = unexpectedBlockedCallback;
  openreq.onsuccess = unexpectedSuccessCallback;
  openreq.onupgradeneeded = function(e) {
    var db = openreq.result;
    var transaction = openreq.transaction;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = unexpectedCompleteCallback;

    var store = db.createObjectStore('store2');
    result('pass - part2 - crash me');

    // Keep adding to the transaction so it can't commit
    (function loop() { store.put(0, 0).onsuccess = loop; }());
  };
}

function testPart3()
{
  // Validate that Part 2 never committed

  // Check version
  var openreq = window.indexedDB.open('version-change-crash');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onblocked = unexpectedBlockedCallback;
  openreq.onupgradeneeded = unexpectedUpgradeNeededCallback;
  openreq.onsuccess = function(e) {
    var db = openreq.result;
    if (db.version !== 1) {
      result('fail - version incorrect');
      return;
    }

    if (!db.objectStoreNames.contains('store1')) {
      result('fail - store1 does not exist');
      return;
    }

    if (db.objectStoreNames.contains('store2')) {
      result('fail - store2 exists');
      return;
    }

    result('pass - part3 - rolled back');
  };
}
