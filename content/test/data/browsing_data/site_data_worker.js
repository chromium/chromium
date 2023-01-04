// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function hasFileSystemAccess() {
  try {
    let dir = await navigator.storage.getDirectory();
    await dir.getFileHandle("worker.txt", { create: false });
    return true;
  } catch (e) {
    return false;
  }
}

async function setFileSystemAccess() {
  try {
    let dir = await navigator.storage.getDirectory();
    await dir.getFileHandle("worker.txt", { create: true });
    return true;
  } catch (e) {
    return false;
  }
}

async function setIndexedDb() {
  return new Promise((resolve) => {
    let open = indexedDB.open('worker_db', 2);
    open.onupgradeneeded = function () {
      open.result.createObjectStore('store');
    }
    open.onsuccess = function () {
      open.result.close();
      resolve(true);
    }
    open.onerror = (e) => {
      resolve(false);
    }
  });
}

async function hasIndexedDb() {
  return new Promise((resolve) => {
    let open = indexedDB.open('worker_db');
    open.onsuccess = function () {
      let hasStore = open.result.objectStoreNames.contains('store');
      open.result.close();
      resolve(hasStore);
    }
    open.onerror = () => resolve(false);
  });
}

async function setCacheStorage() {
  try {
    let cache = await caches.open("worker_cache")
    await cache.put("/foo", new Response("bar"))
    return true;
  } catch {
    return false;
  }
}

async function hasCacheStorage() {
  try {
    let cache = await caches.open("worker_cache")
    let keys = await cache.keys()
    return keys.length > 0;
  } catch {
    return false;
  }
}

onmessage = async function (e) {
  let result = await this[e.data]();
  postMessage(result);
}