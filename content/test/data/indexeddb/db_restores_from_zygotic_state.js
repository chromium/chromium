// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const DB_NAME = 'db-restores-from-zygotic-state';

const STORE_NAME = 'store';
const BLOB_KEY = 'blob-key';
const BLOB_DATA = 'This is the blob content that should survive deletion.';

const STORE_NAME2 = 'store2';
const SECOND_KEY = 'second-key';
const SECOND_DATA = 'Second row of data after recreating the database.';

// Handle to the blob that is read out of the database.
let blob = null;

async function test() {
  // Create database and write a blob.
  let db = await promiseDeleteThenOpenDb(DB_NAME, (database) => {
    database.createObjectStore(STORE_NAME, { keyPath: 'id' })
        .put({id: BLOB_KEY, blob: new Blob([BLOB_DATA])});
  }, 2);

  // Read the blob and hold onto a reference.
  blob = (await readRecord(db, STORE_NAME, BLOB_KEY)).blob;

  // Close connection and delete the database.
  db.close();
  db = null;
  setTimeout(deleteAndRecreateDb);
}

async function deleteAndRecreateDb() {
  await deleteDatabase(DB_NAME);

  // Read and verify the blob contents, which should still work after the DB is
  // deleted.
  const blobText = await readBlobAsText(blob);
  if (blobText !== BLOB_DATA) {
    fail(`Expected blob to contain '${BLOB_DATA}', got '${blobText}'`);
  }

  // Open a *new* database with the same name.
  let db = await promiseOpenDb(DB_NAME, (database, txn) => {
    database.createObjectStore(STORE_NAME2, { keyPath: 'id2' })
      .put({ id2: SECOND_KEY, data: SECOND_DATA });
  }, 1);

  // Can read from the new database.
  const newRecord = await readRecord(db, STORE_NAME2, SECOND_KEY);
  if (newRecord.data !== SECOND_DATA) {
    fail(`Expected new record data to be '${SECOND_DATA}', got '${
        newRecord.data}'`);
  }

  // Verify original blob is still readable.
  const blobText2 = await readBlobAsText(blob);
  if (blobText2 !== BLOB_DATA) {
    fail(`Expected blob to still contain '${BLOB_DATA}', got '${blobText2}'`);
  }

  // When there are no active blobs or IDB connections, the backend should
  // destroy the backing store DB object (`DatabaseConnection`).
  blob = null;
  db.close();
  db = null;
  setTimeout(gc);
  setTimeout(verifyContentsOfRecreatedDb);
}

async function verifyContentsOfRecreatedDb() {
  let db = await promiseOpenDb(DB_NAME, (database, txn) => {
    fail('Didn\'t expect upgradeNeeded on existing DB');
  }, 1);
  const verifyRecord = await readRecord(db, STORE_NAME2, SECOND_KEY);
  if (!verifyRecord || verifyRecord.data !== SECOND_DATA) {
    fail(`Expected persisted data to be '${SECOND_DATA}', got '${
        verifyRecord?.data}'`);
  }

  try {
    // Verify the original store didn't somehow survive.
    db.transaction(STORE_NAME);
    fail('Expected original object store to be gone.');
  } catch (e) {
    done();
  }
}

function deleteDatabase(dbName) {
  return new Promise((resolve, reject) => {
    const request = indexedDB.deleteDatabase(dbName);
    request.onerror = () =>
        reject(new Error(`Failed to delete database ${dbName}`));
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = resolve;
  });
}

function putRecord(db, storeName, record) {
  return new Promise((resolve, reject) => {
    const objectStore = transaction.objectStore(storeName);
    const request = objectStore.put(record);
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = resolve;
  });
}

function readRecord(db, storeName, key) {
  return new Promise((resolve, reject) => {
    try {
      const transaction = db.transaction(storeName, 'readonly');
      transaction.onabort = unexpectedAbortCallback;
      transaction.onerror = unexpectedErrorCallback;
      const objectStore = transaction.objectStore(storeName);
      const request = objectStore.get(key);
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = () => resolve(request.result);
    } catch (e) {
      fail(e);
    }
  });
}

function readBlobAsText(blob) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => {
      fail('Error reading blob');
      reject(new Error('Error reading blob'));
    };
    reader.readAsText(blob);
  });
}
