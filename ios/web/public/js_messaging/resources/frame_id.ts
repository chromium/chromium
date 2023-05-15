// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Returns the frameId associated with this frame. A new value will be created
 * for this frame the first time it is called. The frameId will persist as long
 * as this JavaScript context lives. For example, the frameId will be the same
 * when navigating 'back' to this frame.
 */
function getFrameId(): string {
  if (!gCrWeb.hasOwnProperty('frameId')) {
    gCrWeb.frameId = generateRandomId();
  }
  return gCrWeb.frameId;
}

/**
 * Generates a 128-bit cryptographically-strong random number. The properties
 * must match base::UnguessableToken, as these values may be deserialized into
 * that class on the C++ side.
 * @return the generated number as a hex string.
 */
function generateRandomId(): string {
  // Generate 128 bit unique identifier.
  const components = new Uint32Array(4);
  window.crypto.getRandomValues(components);
  let id = '';
  for (const component of components) {
    // Convert value to base16 string, add leading zeroes if needed (32 bits
    // is 8 hex digits), and append to the ID.
    id += component.toString(16).padStart(8, '0');
  }
  return id;
};

/**
 * Registers this frame by sending its frameId to the native application.
 */
function registerFrame() {
  sendWebKitMessage('FrameBecameAvailable', {'crwFrameId': getFrameId()});
};


export {getFrameId, generateRandomId, registerFrame};
