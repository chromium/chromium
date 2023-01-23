// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used for bi-directional communication between frames and
 * the native code.
 */

// Requires functions from base.js and common.js

/**
 * Namespace for this module.
 */
__gCrWeb.message = {};

// Store message namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['message'] = __gCrWeb.message;

/**
 * Unique identifier for this frame.
 * @type {?string}
 * @private
 */
var frameId_ = null;

/**
 * Returns the frameId associated with this frame. A new value will be created
 * for this frame the first time it is called. The frameId will persist as long
 * as this JavaScript context lives. For example, the frameId will be the same
 * when navigating 'back' to this frame.
 * @return {string} A string representing a unique identifier for this frame.
 */
__gCrWeb.message['getFrameId'] = function() {
  if (!frameId_) {
    // Generate 128 bit unique identifier.
    var components = new Uint32Array(4);
    window.crypto.getRandomValues(components);
    frameId_ = '';
    for (var i = 0; i < components.length; i++) {
      // Convert value to base16 string and append to the |frameId_|.
      frameId_ += components[i].toString(16);
    }
  }
  return /** @type {string} */ (frameId_);
};

/**
 * Registers this frame by sending its frameId to the native application.
 */
__gCrWeb.message['registerFrame'] = function() {
  __gCrWeb.common.sendWebKitMessage(
      'FrameBecameAvailable', {'crwFrameId': __gCrWeb.message['getFrameId']()});
};

/**
 * Registers this frame with the native code and forwards the message to any
 * child frames.
 * This needs to be called by the native application on each navigation
 * because no JavaScript events are fired reliably when a page is displayed and
 * hidden. This is especially important when a page remains alive and is re-used
 * from the WebKit page cache.
 * TODO(crbug.com/872134): In iOS 12, the JavaScript pageshow and pagehide
 *                         events seem reliable, so replace this exposed
 *                         function with a pageshow event listener.
 */
__gCrWeb.message['getExistingFrames'] = function() {
  __gCrWeb.message['registerFrame']();

  var framecount = window['frames']['length'];
  for (var i = 0; i < framecount; i++) {
    window.frames[i].postMessage(
        {type: 'org.chromium.registerForFrameMessaging',},
        '*'
    );
  }
};
