// Copyright 2021 The Chromium Authors. All rights reserved.
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

onmessage = async function (e) {
  let result = await this[e.data]();
  postMessage(result);
}