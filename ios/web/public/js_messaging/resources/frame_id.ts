// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Registers this frame by sending its frameId to the native application.
 */
export function registerFrame() {
  sendWebKitMessage(
    'FrameBecameAvailable', {'crwFrameId': gCrWeb.getFrameId()});
}
