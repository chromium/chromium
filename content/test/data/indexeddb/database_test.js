// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  indexedDBTest(populateObjectStore);
}

function populateObjectStore()
{
  db = event.target.result;
  debug('Populating object store');
  window.objectStore = db.createObjectStore('employees', {keyPath: 'id'});
  shouldBe("objectStore.name", "'employees'");
  shouldBe("objectStore.keyPath", "'id'");

  shouldBe('db.name', 'dbname');
  shouldBe('db.version', '1');
  shouldBe('db.objectStoreNames.length', '1');
  shouldBe('db.objectStoreNames[0]', '"employees"');

  debug('Deleting an object store.');
  db.deleteObjectStore('employees');
  shouldBe('db.objectStoreNames.length', '0');

  done();
}
