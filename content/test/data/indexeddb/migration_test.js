// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  request = indexedDB.open('open-close-version-test1');
  request.onsuccess = openTest2;
  request.onupgradeneeded = onupgrade;
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
}

var saw_upgradeneeded_event = false;
function onupgrade(event) {
  saw_upgradeneeded_event = true;
  db = event.target.result;
  debug("Ensure that the existing leveldb files are used. If they are not, " +
        "this script will create a new database that has no object stores");
  shouldBe("event.oldVersion", "0");
  shouldBe("event.newVersion", "1");
  // A pre-existing store in a database that was version 0 can only
  // happen with databases creating with the old setVersion(string)
  // API that was removed from Chrome circa 2012, and was never
  // supported in other browsers.
  shouldBe("db.objectStoreNames.length", "1");
  shouldBeEqualToString("typeof db.version", "number");
  shouldBe("db.version", "1");
}

function openTest2(event) {
  shouldBeTrue("saw_upgradeneeded_event");
  db = event.target.result;
  shouldBe("db.objectStoreNames.length", "1");
  shouldBeEqualToString("typeof db.version", "number");
  shouldBe("db.version", "1");
  request = indexedDB.open('open-close-version-test2');
  request.onsuccess = done;
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
}
