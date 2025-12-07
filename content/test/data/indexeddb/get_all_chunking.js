// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let connection;
let recordCount;

function populateObjectStore() {
  connection = event.target.result;
  let store = connection.createObjectStore('store-name', null);

  const urlParams = new URLSearchParams(window.location.search);
  recordCount = 2.5 * parseInt(urlParams.get('chunk_size'));
  for (let i = 0; i < recordCount; i++) {
    store.add('value-' + i, i);
  }
}

function doTest() {
  const req = connection.transaction('store-name', 'readonly')
    .objectStore('store-name')
    .getAll();
  req.onerror = () => fail('getAll request should succeed');
  req.onsuccess = evt => {
    if (evt.target.result.length !== recordCount) {
      fail(
        'getAll returned wrong number of records: ' +
        evt.target.result.length + ' vs ' + recordCount);
      return;
    }
    for (let i = 0; i < recordCount; i++) {
      if (evt.target.result[i] !== 'value-' + i) {
        fail(
          'getAll returned wrong value at index i: ' + i +
          ' value: ' + evt.target.result[i]);
        return;
      }
    }

    done();
  };
}

function test() {
  indexedDBTest(populateObjectStore, doTest);
}
