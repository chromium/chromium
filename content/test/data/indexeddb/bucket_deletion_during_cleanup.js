// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function test() {
  try {
    const bucket = await navigator.storageBuckets.open('test-bucket');

    // Open a database and create an object store.
    const db = await new Promise((resolve, reject) => {
      const request = bucket.indexedDB.open('testdb', 1);
      request.onerror = () => reject(request.error);
      request.onsuccess = () => resolve(request.result);
      request.onupgradeneeded = () => {
        request.result.createObjectStore('store');
      };
    });

    // Write data to create database content.
    await new Promise((resolve, reject) => {
      const txn = db.transaction('store', 'readwrite');
      const store = txn.objectStore('store');
      // Write ~1MB of random data.
      const data =
          new Uint8Array(100000).map(() => Math.floor(Math.random() * 256));
      for (let i = 0; i < 10; i++) {
        store.put(data, i);
      }
      txn.oncomplete = resolve;
      txn.onerror = () => reject(txn.error);
    });

    // Clear the data to create free space that needs some sort of cleanup.
    await new Promise((resolve, reject) => {
      const txn = db.transaction('store', 'readwrite');
      txn.objectStore('store').clear();
      txn.oncomplete = resolve;
      txn.onerror = () => reject(txn.error);
    });

    // Close the database and immediately delete the bucket.
    db.close();
    await navigator.storageBuckets.delete('test-bucket');
    done();
  } catch (e) {
    fail('Test failed: ' + e.name + ': ' + e.message);
  }
}
