// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
let db;
const dbName = 'testdb';
const dbVersion = 1;
const worker = new Worker('/back_forward_cache/worker_with_indexedDB.js');

function setupIndexedDBConnection() {
  let request = window.indexedDB.open(dbName, dbVersion);
  return new Promise((resolve, reject) => {
    request.onsuccess = (event) => {
      db = event.target.result;
      resolve();
    };
    request.onerror = (error) => {
      reject(error);
    };
    request.onupgradeneeded = (event) => {
      event.target.result.createObjectStore('store', {autoIncrement: true});
    };
  });
}

function createIndexedDBTransaction() {
  let transaction = db.transaction(['store'], 'readwrite');
  transaction.oncomplete = () => {
    window.domAutomationController.send('transaction_completed');
  };
  return [transaction, transaction.objectStore('store')];
}

function runInfiniteIndexedDBTransactionLoop() {
  const [_, store] = createIndexedDBTransaction();
  const infiniteLoop = () => {
    let request = store.put('key', 'value');
    request.onsuccess = infiniteLoop;
  };
  infiniteLoop();
}

function sendMessageToWorker(message) {
  return new Promise(resolve => {
    worker.addEventListener('message', (msg) => {
      resolve(msg.data);
    });
    worker.postMessage(message);
  });
}
