// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testDate = new Date("February 24, 1955 12:00:00");

function getByDateSuccess()
{
  debug('Data retrieved by date key');

  shouldBe("event.target.result", "'foo'");
  transaction.oncomplete = done;
}

function recordNotFound()
{
  debug('Removed data can no longer be found');
  shouldBe("event.target.result", "undefined");

  debug('Retrieving an index');
  shouldBe("objectStore.index('fname_index').name", "'fname_index'");

  debug('Removing an index');
  try {
    objectStore.deleteIndex('fname_index');
  } catch(e) {
    fail(e);
  }

  var request = transaction.objectStore('stuff').get(testDate);
  request.onsuccess = getByDateSuccess;
  request.onerror = unexpectedErrorCallback;
}

function removeSuccess()
{
  debug('Data removed');

  var request = objectStore.get(1);
  request.onsuccess = recordNotFound;
  request.onerror = unexpectedSuccessCallback;
}

function getSuccess()
{
  debug('Data retrieved');

  shouldBe("event.target.result.fname", "'John'");
  shouldBe("event.target.result.lname", "'Doe'");
  shouldBe("event.target.result.id", "1");

  var request = objectStore.delete(1);
  request.onsuccess = removeSuccess;
  request.onerror = unexpectedErrorCallback;
}

function moreDataAddedSuccess()
{
  debug('More data added');

  var request = objectStore.get(1);
  request.onsuccess = getSuccess;
  request.onerror = unexpectedErrorCallback;
}

function addWithSameKeyFailed()
{
  debug('Adding a record with same key failed');
  shouldBe("event.target.error.name", "'ConstraintError'");
  event.preventDefault();

  var request = transaction.objectStore('stuff').add('foo', testDate);
  request.onsuccess = moreDataAddedSuccess;
  request.onerror = unexpectedErrorCallback;
}

function dataAddedSuccess()
{
  debug('Data added');

  debug('Try to add employee with same id');
  var request = objectStore.add({fname: "Tom", lname: "Jones", id: 1});
  request.onsuccess = unexpectedSuccessCallback;
  request.onerror = addWithSameKeyFailed;
}

function populateObjectStore()
{
  window.transaction = event.target.transaction;
  db = event.target.result;
  transaction.onabort = unexpectedAbortCallback;
  debug('Populating object store');
  deleteAllObjectStores(db);
  db.createObjectStore('stuff');
  window.objectStore = db.createObjectStore('employees', {keyPath: 'id'});
  shouldBe("objectStore.name", "'employees'");
  shouldBe("objectStore.keyPath", "'id'");

  objectStore.createIndex('fname_index', 'fname');
  objectStore.createIndex('lname_index', 'fname');
  debug('Created indexes');
  shouldBe("objectStore.indexNames[0]", "'fname_index'");
  shouldBe("objectStore.indexNames[1]", "'lname_index'");

  var request = objectStore.add({fname: "John", lname: "Doe", id: 1});
  request.onsuccess = dataAddedSuccess;
  request.onerror = unexpectedErrorCallback;
}

function test() {
  indexedDBTest(populateObjectStore);
}
