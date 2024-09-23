// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used for bi-directional communication between frames and
 * the native code.
 */

import {getFrameId, registerFrame} from '//ios/web/public/js_messaging/resources/frame_id.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Registers this frame with the native code and forwards the message to any
 * child frames.
 * This needs to be called by the native application on each navigation
 * because no JavaScript events are fired reliably when a page is displayed and
 * hidden. This is especially important when a page remains alive and is re-used
 * from the WebKit page cache.
 * TODO(crbug.com/41406778): In iOS 12, the JavaScript pageshow and pagehide
 *                         events seem reliable, so replace this exposed
 *                         function with a pageshow event listener.
 */
function getExistingFrames() {
  registerFrame();

  const framecount = window.frames.length;
  for (let i = 0; i < framecount; i++) {
    const frame = window.frames[i];
    if (!frame) {
      continue;
    }

    frame.postMessage({type: 'org.chromium.registerForFrameMessaging'}, '*');
  }
};

gCrWeb.message = {
  getFrameId,
  getExistingFrames
};
