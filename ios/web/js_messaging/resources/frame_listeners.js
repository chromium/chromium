// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Requires functions from common.js and message.js.

window.addEventListener('unload', function(event) {
  __gCrWeb.common.sendWebKitMessage('FrameBecameUnavailable',
      __gCrWeb.message.getFrameId());
});

/**
 * Listens for messages received by the parent frame to initialize messaging
 * state.
 */
window.addEventListener('message', function(message) {
  var payload = message.data;
  if (typeof payload !== 'object') {
    return;
  }
  if (payload.hasOwnProperty('type') &&
    payload.type == 'org.chromium.registerForFrameMessaging') {
    __gCrWeb.message['getExistingFrames']();
  }
});
