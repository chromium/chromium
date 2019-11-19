// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy

/**
 * @fileoverview Notifies the application that this frame has loaded. This file
 * must be included at document end time.
 */

goog.provide('__crWeb.setupFrame');

// Requires __crWeb.common and __crWeb.message provided by
// __crWeb.allFramesWebBundle.

/* Beginning of anonymous object. */
(function() {

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
  } else if (payload.hasOwnProperty('type') &&
      payload.type == 'org.chromium.encryptedMessage') {
    if (payload.hasOwnProperty('message_payload') &&
        payload.hasOwnProperty('function_payload') &&
        payload.hasOwnProperty('target_frame_id')) {
      __gCrWeb.message['routeMessage'](
        payload['message_payload'],
        payload['function_payload'],
        payload['target_frame_id']
      );
    }
  }
});

// Frame registration must be delayed until Document End script injection time.
// (This file is injected at that time, but the message API is defined at
// Document Start time.)
// TODO(crbug.com/873730): Stop exposing registerFrame API.
__gCrWeb.message['registerFrame']();

}());  // End of anonymous object
