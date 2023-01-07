// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

// This function is called by common.js when a message is received from the
// NaCl module.
function handleMessage(message) {
  var logEl = document.getElementById('log');
  logEl.textContent += message.data;
}
