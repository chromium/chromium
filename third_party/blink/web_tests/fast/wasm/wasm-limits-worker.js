// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.importScripts("wasm-constants.js");
this.importScripts("wasm-module-builder.js");
this.importScripts("wasm-limits-tests-common.js");

onmessage = function(limit) {
  var buffer = createTestBuffers(limit).large;
  var m = undefined;
  var i = undefined;
  try {
    m = new WebAssembly.Module(buffer);
    i = new WebAssembly.Instance(m);
  } catch (e) {
    postMessage(false);
  }
  postMessage(m instanceof WebAssembly.Module &&
              i instanceof WebAssembly.Instance);
}
