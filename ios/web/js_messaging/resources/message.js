// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used for bi-directional communication between frames and
 * the native code.
 */

goog.provide('__crWeb.message');

goog.require('__crWeb.base');
goog.require('__crWeb.common');

/**
 * Namespace for this module.
 */
__gCrWeb.message = {};

// Store message namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['message'] = __gCrWeb.message;

/* Beginning of anonymous object. */
(function() {
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
 * The encryption key to decrypt messages received from native code.
 * @type {?webCrypto.CryptoKey}
 * @private
 */
var frameSymmetricKey_ = null;

/**
 * The ID of the last processed message. If a received message has an ID less
 * than or equal to |lastReceivedMessageId_|, it will be ignored.
 * @type {number}
 * @private
 */
var lastReceivedMessageId_ = -1;

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
 * Returns whether or not frame messaging is supported for this frame.
 * @returns true if frame messaging is supported, false otherwise.
 */
var isFrameMessagingSupported_ = function() {
  // - Only secure contexts support the crypto.subtle API.
  return window.isSecureContext && typeof window.crypto.subtle === 'object';
}

/**
 * Exports |frameSymmetricKey_| as a base64 string. Key will be created if it
 * does not already exist.
 * @param {function(string)} callback A callback to be run with the exported
 *                           base64 key.
 */
var exportKey_ = function(callback) {
  // Early return with an empty key string if encryption is not supported in
  // this frame.
  if (!isFrameMessagingSupported_()) {
    callback("");
    return;
  }
  try {
    getFrameSymmetricKey_(function(key) {
      window.crypto.subtle.exportKey('raw', key)
          .then(function(/** @type {!ArrayBuffer|!webCrypto.JsonWebKey} */ k) {
        var keyBytes = new Uint8Array(/** @type {!ArrayBuffer} */ (k));
        var key64 = btoa(String.fromCharCode.apply(null, keyBytes));
        callback(key64);
      });
    });
  } catch (error) {
    // AES-GCM will not be supported if a web developer overrode
    // window.crypto.subtle with window.crypto.webkitSubtle on iOS 10.
    callback("");
  }
};

/**
 * Runs |callback| with the key associated with this frame. The key will be
 * created if necessary. The key will persist as long as this JavaScript context
 * lives. For example, the key will be the same when navigating 'back' to this
 * frame.
 * @param {function(!webCrypto.CryptoKey)} callback A callback to be run with
 *                                         the key.
 */
var getFrameSymmetricKey_ = function(callback) {
  if (frameSymmetricKey_) {
    callback(frameSymmetricKey_);
    return;
  }
  window.crypto.subtle.generateKey(
    {'name': 'AES-GCM', 'length': 256},
    true,
    ['decrypt', 'encrypt']
  ).then(function(
      /** @type {!webCrypto.CryptoKey|!webCrypto.CryptoKeyPair} */ key) {
    frameSymmetricKey_ = /** @type {!webCrypto.CryptoKey} */ (key);
    callback(frameSymmetricKey_);
  });
};


/**
 * Sends |result| to the native application as the return value from the
 * execution of the message with |messageId|.
 * @param {number} messageId The message ID which the response is associated.
 * @param {?Object} result The response to send.
 */
var replyWithResult_ = function(messageId, result) {
  var replyCommand = 'frameMessaging_' +
      __gCrWeb.message['getFrameId']() + '.reply';
  var response = {
    'command': replyCommand,
    'messageId': messageId
  };
  if (typeof result !== 'undefined') {
    response['result'] = result
  }
  __gCrWeb.message.invokeOnHost(response);
};

/**
 * Executes |functionName| on __gCrWeb with the given |parameters|.
 * @param {!string} functionPath The function to execute on __gCrWeb. Components
 *                  may be separated by periods. For example: messaging.function
 * @param {!Array} parameters The parameters to pass to |functionName|.
 * @return The return value of executing |functionName| or null if it couldn't
 *         be executed.
 */
var callGCrWebFunction_ = function(functionPath, parameters) {
  var functionReference = __gCrWeb;
  var functionComponents = functionPath.split('.');
  var numComponents = functionComponents.length;
  for (var i = 0; i < numComponents; i++) {
    var component = functionComponents[i];
    functionReference = functionReference[component];
    if (!functionReference) {
      return null;
    }
  }
  return functionReference.apply(null, parameters);
}

/**
 * Decrypts and executes the function specified in |functionPayload|.
 * @param {Object} encryptedMessageDetails JSON containing encrypted
 * information about the message and the initialization vector to decrypt
 * the information.
 * @param {!Object} encryptedFunctionDetails JSON containing encrypted
 * information about the function and its parameters and the initialization
 * vector to decrypt the information. If null, won't execute any call, but
 * will respond to the native call if specified in |encryptedMessageDetails|.
 */
var executeMessage_ = function(encryptedMessageDetails,
  encryptedFunctionDetails) {
  if (!frameSymmetricKey_) {
    // Payload cannot be decrypted without a key. This message could be spam or
    // sent by the native application by mistake.
    return;
  }

  // Decode the base64 payload.
  var encryptedMessageArray = new Uint8Array(Array.from(
      atob(encryptedMessageDetails['payload'])).map(function(a) {
    return a.charCodeAt(0);
  }));

  // Decode the base64 initialization buffer.
  var messageIvbuf = new Uint8Array(Array.from(
    atob(encryptedMessageDetails['iv'])).map(function(a) {
    return a.charCodeAt(0);
  }));
  var messageAlgorithm = {'name': 'AES-GCM', iv: messageIvbuf};
  getFrameSymmetricKey_(function(frameKey) {
    window.crypto.subtle.decrypt(
      messageAlgorithm, frameKey, encryptedMessageArray)
      .then(function(decryptedMessagePayload) {
      var messageJSONString = new TextDecoder().decode(
        new Uint8Array(decryptedMessagePayload));
      var messageDict = JSON.parse(messageJSONString);

      // Verify that message id is valid.
      if (!Number.isInteger(messageDict['messageId']) ||
          messageDict['messageId'] <= lastReceivedMessageId_) {
        return;
      }

      lastReceivedMessageId_ = messageDict['messageId'];

      // Return early if the function payload was dropped.
      if (!encryptedFunctionDetails) {
        if (typeof messageDict['replyWithResult'] === 'boolean' &&
          messageDict['replyWithResult']) {
          replyWithResult_(messageDict['messageId'], null);
        }
        return;
      }

      // Decode the base64 payload.
      var encryptedFunctionArray = new Uint8Array(Array.from(
        atob(encryptedFunctionDetails['payload'])).map(
        function(a) {
          return a.charCodeAt(0);
      }));

      // Decode the base64 initialization buffer.
      var functionIvbuf = new Uint8Array(Array.from(
        atob(encryptedFunctionDetails['iv'])).map(
        function(a) {
          return a.charCodeAt(0);
      }));

      var additionalData = new Uint8Array(Array.from(
        messageDict['messageId'].toString()).map(
        function(a) {
          return a.charCodeAt(0);
      }));
      var functionAlgorithm = {'name': 'AES-GCM',
       iv: functionIvbuf,
       additionalData: additionalData};
      window.crypto.subtle.decrypt(
        functionAlgorithm, frameKey, encryptedFunctionArray)
        .then(function(decryptedFunctionPayload) {
        var functionJSONPayload = new TextDecoder().decode(
          new Uint8Array(decryptedFunctionPayload));
        var functionDict = JSON.parse(functionJSONPayload);

        let functionName = functionDict['functionName'];
        let parameters = functionDict['parameters'];

        var result = null;
        if (typeof functionName === 'string' && functionName.length >= 1
         && Array.isArray(parameters)) {
            result = callGCrWebFunction_(functionName, parameters);
        }
        if (typeof messageDict['replyWithResult'] === 'boolean'
         && messageDict['replyWithResult']) {
          replyWithResult_(messageDict['messageId'], result);
        }
      });
    });
  });
};

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
 * Routes an encrypted message to the targeted frame. Once the target frame is
 * found, the |payload| will be decrypted and executed. This function is called
 * by the native code.
 * @param {!Object} encryptedMessageDetails  JSON representing a dictionary
 * containing encrypted information about the message and the initialization
 * vector to decrypt the information.
 * @param {!Object} encryptedFunctionDetails JSON representing a dictionary
 * containing encrypted information about the function to call and the
 * parameters to pass and the initialization vector to decrypt the information.
 * @param {!string} target_frame_id The |frameId_| of the frame which should
 *                  process the |payload|.
 */
__gCrWeb.message['routeMessage'] = function(encryptedMessageDetails,
  encryptedFunctionDetails, target_frame_id) {
  if (!isFrameMessagingSupported_()) {
    // API is unsupported.
    return;
  }

  if (target_frame_id === __gCrWeb.message['getFrameId']()) {
    executeMessage_(encryptedMessageDetails, encryptedFunctionDetails);
    return;
  }

  var framecount = window.frames.length;
  for (var i = 0; i < framecount; i++) {
    window.frames[i].postMessage(
      {
        type: 'org.chromium.encryptedMessage',
        message_payload: encryptedMessageDetails,
        function_payload: encryptedFunctionDetails,
        target_frame_id: target_frame_id
      },
      '*'
    );
  }
};

/**
 * Creates (or gets the existing) encryption key and sends it to the native
 * application.
 */
__gCrWeb.message['registerFrame'] = function() {
  exportKey_(function(frameKey) {
    __gCrWeb.common.sendWebKitMessage('FrameBecameAvailable', {
      'crwFrameId': __gCrWeb.message['getFrameId'](),
      'crwFrameKey': frameKey,
      'crwFrameLastReceivedMessageId': lastReceivedMessageId_
    });
    // Allow messaging now that the frame has been registered and send any
    // already queued messages.
    messaging_suspended_ = false;
    __gCrWeb.message.invokeQueues();
  });
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

}());
