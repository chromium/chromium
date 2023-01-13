// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
let db;
const dbName = 'testdb';
const dbVersion = 1;

async function setupIndexedDBConnection() {
  db = await new Promise((resolve, reject) => {
    let request = window.indexedDB.open(dbName, dbVersion);
    request.onsuccess = () => resolve(request.result);
    request.onerror = (error) => reject(error);;
    request.onupgradeneeded = () => {
      request.result.createObjectStore('store');
    };
  });
}

async function setupNewIndexedDBConnectionWithSameVersion() {
  let db2 = await new Promise((resolve, reject) => {
    let request = window.indexedDB.open(dbName, dbVersion);
    request.onsuccess = () => resolve(request.result);
    request.onerror = (error) => reject(error);
  });
}

async function setupNewIndexedDBConnectionWithHigherVersion() {
  let db3 = await new Promise((resolve, reject) => {
    let request = window.indexedDB.open(dbName, dbVersion + 1);
    request.onsuccess = () => {
      window.domAutomationController.send('onsuccess');
      resolve(request.result);
    };
    request.onerror = (error) => reject(error);
  });
}

async function setupIndexedDBVersionChangeHandlerToCloseConnectionAndNavigateTo(url) {
  let db4 = await new Promise((resolve, reject) => {
    let request = window.indexedDB.open(dbName, dbVersion);
    request.onsuccess = () => resolve(request.result);
    request.onerror = (error) => reject(error);
  });
  db4.onversionchange = (event) => {
    db4.close();
    window.domAutomationController.send('onversionchange');
    location.href = url;
  };
}

async function setupIndexedDBVersionChangeHandlerToNavigateTo(url) {
  let db4 = await new Promise((resolve, reject) => {
    let request = window.indexedDB.open(dbName, dbVersion);
    request.onsuccess = () => resolve(request.result);
    request.onerror = (error) => reject(error);
  });
  db4.onversionchange = (event) => {
    window.domAutomationController.send('onversionchange');
    location.href = url;
  };
}

function startIndexedDBTransaction() {
  let transaction = db.transaction(['store'], 'readwrite');
  let store = transaction.objectStore('store');
  store.put("key", "value");
}

function runInfiniteIndexedDBTransactionLoop() {
  let transaction = db.transaction(['store'], 'readwrite');
  let store = transaction.objectStore('store');
  let infiniteLoop = () => {
    let request = store.put("key", "value");
    request.onsuccess = infiniteLoop;
  }
  infiniteLoop();
}

function registerPagehideToCloseIndexedDBConnection() {
  addEventListener('pagehide', () => {
    db.close();
  });
}

function registerPagehideToStartTransaction() {
  addEventListener('pagehide', () => {
    let transaction = db.transaction(['store'], 'readwrite');
    let store = transaction.objectStore('store');
    store.put("key", "value");

    // Queue a request to close the connection.
    db.close();
  });
}

function registerPagehideToStartAndCommitTransaction() {
  addEventListener('pagehide', () => {
    let transaction = db.transaction(['store'], 'readwrite');
    let store = transaction.objectStore('store');
    store.put("key", "value");

    // Call commit to run the transaction right away.
    transaction.commit();
    // Close the connection.
    db.close();
  });
}
