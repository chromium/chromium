// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {generateRandomId, sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Returns the frameId associated with this frame. A new value will be created
 * for this frame the first time it is called. The frameId will persist as long
 * as this JavaScript context lives. For example, the frameId will be the same
 * when navigating 'back' to this frame.
 */
export function getFrameId(): string {
  if (!gCrWeb.hasOwnProperty('frameId')) {
    gCrWeb.frameId = generateRandomId();
  }
  return gCrWeb.frameId;
}

/**
 * Registers this frame by sending its frameId to the native application.
 */
export function registerFrame() {
  sendWebKitMessage('FrameBecameAvailable', {'crwFrameId': getFrameId()});
}
