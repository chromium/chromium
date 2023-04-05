// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = {};
function victim() {}

Object.defineProperty(obj, 'handleEvent', {
  get: () => {
    // Remove the victim function from the listener vector to break the loop.
    self.removeEventListener('fetch', victim);
    return () => {};
  },
  configurable: true,
  enumerable: true,
});

self.addEventListener('fetch', obj);
self.addEventListener('fetch', victim);
