// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let db;
let reader;
let bytesRead = 0;

async function openDatabase(name, create = false) {
  db = await new Promise((resolve, reject) => {
    const request = indexedDB.open(name, 1);
    request.onupgradeneeded = () => {
      if (create) {
        request.result.createObjectStore('blobs');
      } else {
        request.transaction.abort();
        reject(new Error('The database should already exist'));
      }
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function createDatabase(name, blobSize) {
  await openDatabase(name, true);
  await new Promise((resolve, reject) => {
    const transaction = db.transaction('blobs', 'readwrite');
    transaction.objectStore('blobs').put(
        new Blob([new Uint8Array(blobSize).fill(97)]), 'blob');
    transaction.oncomplete = resolve;
    transaction.onabort = () => reject(transaction.error);
  });
}

async function readFirstChunk() {
  const blob = await new Promise((resolve, reject) => {
    const request = db.transaction('blobs').objectStore('blobs').get('blob');
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  reader = blob.stream().getReader();
  const {value, done} = await reader.read();
  if (done || value.byteLength === 0) {
    throw new Error('Expected the first blob chunk');
  }
  bytesRead = value.byteLength;
  return bytesRead;
}

async function readUntilError() {
  try {
    while (true) {
      const {value, done} = await reader.read();
      if (done) {
        return 'Unexpected end of stream';
      }
      bytesRead += value.byteLength;
    }
  } catch (error) {
    return error.name;
  }
}

function getBytesRead() {
  return bytesRead;
}
