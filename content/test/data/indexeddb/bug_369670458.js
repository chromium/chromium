// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let connection;
let file;

function populateObjectStore() {
  connection = event.target.result;
  let store = connection.createObjectStore('store-name', null);
}

function doTest() {
  const txn =
      connection.transaction('store-name', 'readwrite');
  txn.objectStore('store-name').add(file.slice(10, -10, file.type), 'key');
  txn.onabort = unexpectedAbortCallback;
  txn.onerror = unexpectedErrorCallback;
  txn.oncomplete = done;
}

function setUp() {
  document
      .getElementById('fileInput')
      .addEventListener('change', (e) => {
    file = e.target.files[0];
    indexedDBTest(populateObjectStore, doTest);
  });
}
