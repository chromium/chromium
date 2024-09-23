// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function afterCommit()
{
    try {
        debug("Accessing a committed transaction should throw");
        var store = transaction.objectStore('storeName');
    } catch (e) {
        exc = e;
        shouldBe('exc.code', 'DOMException.INVALID_STATE_ERR');
    }
    done();
}

function nonExistingKey()
{
    shouldBe("event.target.result", "undefined");
    transaction.oncomplete = afterCommit;
}

function gotValue()
{
    value = event.target.result;
    shouldBeEqualToString('value', 'myValue');
}

function startTransaction()
{
    debug("Using get in a transaction");
    transaction = db.transaction('storeName');
    store = transaction.objectStore('storeName');
    shouldBeEqualToString("store.name", "storeName");
    request = store.get('myKey');
    request.onsuccess = gotValue;
    request.onerror = unexpectedErrorCallback;

    var emptyRequest = store.get('nonExistingKey');
    emptyRequest.onsuccess = nonExistingKey;
    emptyRequest.onerror = unexpectedErrorCallback;
}

function populateObjectStore()
{
    db = event.target.result;
    deleteAllObjectStores(db);
    window.objectStore = db.createObjectStore('storeName');
    var request = objectStore.add('myValue', 'myKey');
    request.onerror = unexpectedErrorCallback;
}

function test() {
  indexedDBTest(populateObjectStore, startTransaction);
}
