// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Reports viewport details to the app.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Reads the viewport configuration and reports it back to the browser.
 */
function reportViewportConfiguration() {
  const viewportMeta = window.document.querySelector('meta[name = "viewport"]');
  if (viewportMeta) {
    const coverValue =
        viewportMeta.getAttribute('content')?.includes('viewport-fit=cover');
    sendWebKitMessage('FullscreenViewportHandler', {
      'frame_id': gCrWeb.getFrameId(),
      'cover': coverValue,
    });
  }
}

window.addEventListener('load', reportViewportConfiguration);
