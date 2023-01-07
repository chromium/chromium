// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  // Do not use indexedDBTest() - need to specify the database name
  var dbname = "doesnt-hang-test";

  var request = indexedDB.deleteDatabase(dbname);
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
  request.onsuccess = function() {
    var request = indexedDB.open(dbname, 1);
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = onUpgradeNeeded;
    request.onsuccess = onOpenSuccess;
  };
}

function onUpgradeNeeded()
{
  // We are now in a set version transaction.
  debug('Creating object store.');
  var db = event.target.result;
  db.createObjectStore('store');
}


var objectStore;
function onOpenSuccess()
{
  var db = event.target.result;

  debug('Creating new transaction.');
  var transaction = db.transaction('store', 'readwrite');
  transaction.oncomplete = unexpectedCompleteCallback;
  transaction.onabort = unexpectedAbortCallback;
  objectStore = transaction.objectStore('store');

  debug('Starting endless loop...');
  endlessLoop();
}

var loopCount = 0;
function endlessLoop()
{
  var request = objectStore.get(0);
  request.onsuccess = endlessLoop;
  request.onerror = unexpectedErrorCallback;

  loopCount += 1;
  if (loopCount == 7) {
    // If we've already looped 7 times, it's pretty safe to assume
    // we'll continue looping for some time...
    debug("Looping infinitely within a transaction.");
    done();
  }
}
