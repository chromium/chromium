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
 * Boolean to track if messaging is suspended. While suspended, messages will be
 * queued and sent once messaging is no longer suspended.
 * @type {boolean}
 * @private
 */
var messaging_suspended_ = true;

/**
 * Object to manage queue of messages waiting to be sent to the main
 * application for asynchronous processing.
 * @type {Object}
 * @private
 */
var messageQueue_ = {
  scheme: 'crwebinvoke',
  reset: function() {
    messageQueue_.queue = [];
    // Since the array will be JSON serialized, protect against non-standard
    // custom versions of Array.prototype.toJSON.
    delete messageQueue_.queue.toJSON;
  }
};
messageQueue_.reset();

/**
 * Unique identifier for this frame.
 * @type {?string}
 * @private
 */
var frameId_ = null;

/**
 * Invokes a command on the Objective-C side.
 * @param {Object} command The command in a JavaScript object.
 * @public
 */
__gCrWeb.message.invokeOnHost = function(command) {
  messageQueue_.queue.push(command);
  sendQueue_(messageQueue_);
};

/**
 * Sends both queues if they contain messages.
 */
__gCrWeb.message.invokeQueues = function() {
  if (messageQueue_.queue.length > 0) sendQueue_(messageQueue_);
};

function sendQueue_(queueObject) {
  if (messaging_suspended_) {
    // Leave messages queued if messaging is suspended.
    return;
  }

  var windowId = null;
  try {
    windowId = window.top.__gCrWeb['windowId'];
    // Do nothing if windowId has not been set.
    if (typeof windowId != 'string') {
      return;
    }
  } catch (e) {
    // A SecurityError will be thrown if this is a cross origin iframe. Allow
    // sending the message in this case and it will be filtered by frameID.
    if (e.name !== 'SecurityError') {
      throw e;
    }
  }

  // Some pages/plugins implement Object.prototype.toJSON, which can result
  // in serializing messageQueue_ to an invalid format.
  var originalObjectToJSON = Object.prototype.toJSON;
  if (originalObjectToJSON) delete Object.prototype.toJSON;

  queueObject.queue.forEach(function(command) {
    var message = {
      'crwCommand': command,
      'crwFrameId': __gCrWeb.message['getFrameId']()
    };
    if (windowId) {
      message['crwWindowId'] = windowId;
    }
    __gCrWeb.common.sendWebKitMessage(queueObject.scheme, message);
  });
  queueObject.reset();

  if (originalObjectToJSON) {
    // Restore Object.prototype.toJSON to prevent from breaking any
    // functionality on the page that depends on its custom implementation.
    Object.prototype.toJSON = originalObjectToJSON;
  }
}

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
  // Allow messaging now that the frame has been registered and send any
  // already queued messages.
  messaging_suspended_ = false;
  __gCrWeb.message.invokeQueues();
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
