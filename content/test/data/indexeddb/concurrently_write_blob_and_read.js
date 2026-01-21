// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/475947902.
// Write a blob, and while that is ongoing, read from another object store and
// make sure that the read transaction commits first.

function upgradeCallback(db) {
  deleteAllObjectStores(db);
  db.createObjectStore('storeWithBlobs', {autoIncrement: true});
  db.createObjectStore('otherStore', {autoIncrement: true});
}

async function test() {
  const db = await promiseOpenDb('blob_db', upgradeCallback);

  const transaction = db.transaction('storeWithBlobs', 'readwrite');
  transaction.onabort = unexpectedAbortCallback;
  transaction.onerror = unexpectedErrorCallback;
  const objectStore = transaction.objectStore('storeWithBlobs');
  const request = objectStore.put({blob: new Blob(['abc'])}, 'key-0');
  request.onerror = unexpectedErrorCallback;

  const readTransaction = db.transaction('otherStore', 'readonly');
  readTransaction.onabort = unexpectedAbortCallback;
  readTransaction.onerror = unexpectedErrorCallback;
  const readRequest =
      readTransaction.objectStore('otherStore').get('nonexistent-key');
  readTransaction.commit();
  readTransaction.oncomplete = done;
}
