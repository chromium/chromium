// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onCursor()
{
  var cursor = event.target.result;
  if (cursor === null) {
    debug('Reached end of object cursor.');
    if (!gotObjectThroughCursor) {
      fail('Did not get object through cursor.');
      return;
    }
    done();
    return;
  }

  debug('Got object through cursor.');
  shouldBe('event.target.result.key', '55');
  shouldBe('event.target.result.value.aValue', '"foo"');
  gotObjectThroughCursor = true;

  cursor.continue();
}

function onKeyCursor()
{
  var cursor = event.target.result;
  if (cursor === null) {
    debug('Reached end of key cursor.');
    if (!gotKeyThroughCursor) {
      fail('Did not get key through cursor.');
      return;
    }

    var request = index.openCursor(IDBKeyRange.only(55));
    request.onsuccess = onCursor;
    request.onerror = unexpectedErrorCallback;
    gotObjectThroughCursor = false;
    return;
  }

  debug('Got key through cursor.');
  shouldBe('event.target.result.key', '55');
  shouldBe('event.target.result.primaryKey', '1');
  gotKeyThroughCursor = true;

  cursor.continue();
}

function getSuccess()
{
  debug('Successfully got object through key in index.');

  shouldBe('event.target.result.aKey', '55');
  shouldBe('event.target.result.aValue', '"foo"');

  var request = index.openKeyCursor(IDBKeyRange.only(55));
  request.onsuccess = onKeyCursor;
  request.onerror = unexpectedErrorCallback;
  gotKeyThroughCursor = false;
}

function getKeySuccess()
{
  debug('Successfully got key.');
  shouldBe('event.target.result', '1');

  var request = index.get(55);
  request.onsuccess = getSuccess;
  request.onerror = unexpectedErrorCallback;
}

function moreDataAdded()
{
  debug('Successfully added more data.');

  var request = index.getKey(55);
  request.onsuccess = getKeySuccess;
  request.onerror = unexpectedErrorCallback;
}

function indexErrorExpected()
{
  debug('Existing index triggered on error as expected.');

  var request = objectStore.put({'aKey': 55, 'aValue': 'foo'}, 1);
  request.onsuccess = moreDataAdded;
  request.onerror = unexpectedErrorCallback;
}

function indexSuccess()
{
  debug('Index created successfully.');

  shouldBe("index.name", "'myIndex'");
  shouldBe("index.objectStore.name", "'test'");
  shouldBe("index.keyPath", "'aKey'");
  shouldBe("index.unique", "true");

  try {
    request = objectStore.createIndex('myIndex', 'aKey', {unique: true});
    fail('Re-creating an index must throw an exception');
  } catch (e) {
    indexErrorExpected();
  }
}

function createIndex(expect_error)
{
  debug('Creating an index.');
  try {
    window.index = objectStore.createIndex('myIndex', 'aKey', {unique: true});
    indexSuccess();
  } catch (e) {
    unexpectedErrorCallback();
  }
}

function dataAddedSuccess()
{
  debug('Data added');
  createIndex(false);
}

function populateObjectStore()
{
  debug('Populating object store');
  db = event.target.result;
  window.objectStore = db.createObjectStore('test');
  var myValue = {'aKey': 21, 'aValue': '!42'};
  var request = objectStore.add(myValue, 0);
  request.onsuccess = dataAddedSuccess;
  request.onerror = unexpectedErrorCallback;
}

function test() {
  indexedDBTest(populateObjectStore);
}

