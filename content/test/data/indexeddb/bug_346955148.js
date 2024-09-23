// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a regression test for crbug.com/346955148. It tests what happens when
// a getAll request is in the queue and the IndexedDBConnection is destroyed
// by way of the renderer dropping the mojo connection. Success occurs if the
// browser doesn't crash.

'use strict';

let connection;

function populateObjectStore() {
  connection = event.target.result;
  let store = connection.createObjectStore('store-name', null);
}

function createGetAllRequest() {
  const transaction = connection.transaction('store-name', 'readonly');
  const store = transaction.objectStore('store-name');
  const req = store.getAll();
  req.onerror = () => fail('getAll request should succeed');
  return req;
}

function createAddRequest() {
  const txn =
      connection.transaction('store-name', 'readwrite', {durability: 'strict'});
  txn.objectStore('store-name').add('value-X', '42');
}

function doTest() {
  createGetAllRequest().onsuccess = evt => {
    // Create a lot of slow requests, with one getAll() waiting behind them.
    for (let i = 0; i < 100; i++) {
      createAddRequest();
    }
    createGetAllRequest().onsuccess = evt => {
      assert_array_equals(evt.target.result, ['value-X']);
    };
    connection = null;
    gc();
    done();
  };
}

function test() {
  indexedDBTest(populateObjectStore, doTest);
}
