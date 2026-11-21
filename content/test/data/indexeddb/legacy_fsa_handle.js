// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function openDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open('legacy-fsa');
    request.onupgradeneeded = () => {
      request.result.createObjectStore('handles');
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function storeHandle() {
  const [handle] = await showOpenFilePicker();
  const db = await openDatabase();
  const tx = db.transaction('handles', 'readwrite');
  tx.objectStore('handles').put(handle, 'file');
  await new Promise((resolve, reject) => {
    tx.oncomplete = resolve;
    tx.onabort = () => reject(tx.error);
  });
  db.close();
}

async function readFile() {
  const db = await openDatabase();
  const tx = db.transaction('handles');
  const handle = await new Promise((resolve, reject) => {
    const request = tx.objectStore('handles').get('file');
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  db.close();
  return (await handle.getFile()).text();
}
