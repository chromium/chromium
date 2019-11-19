// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  request = indexedDB.open('database-basics');
  request.onupgradeneeded = upgradeNeeded;
  request.onsuccess = onSuccess;
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
}

var gotUpgradeNeeded = false;
function upgradeNeeded(evt) {
  event = evt;
  shouldBe("event.dataLoss", "'total'");
  shouldBe("event.dataLossMessage",
           "'IndexedDB (database was corrupt): missing files'");
  gotUpgradeNeeded = true;
}

function onSuccess(event) {
  db = event.target.result;
  debug("The pre-existing leveldb has an objectStore in 'database-basics',");
  debug("ensure that it was blown away");
  shouldBe("db.objectStoreNames.length", "0");
  debug("We should have gotten an upgradeneeded event because the new empty");
  debug("database doesn't have a version.");
  shouldBeTrue("gotUpgradeNeeded");
  done();
}
