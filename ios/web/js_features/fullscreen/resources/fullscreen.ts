// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Reports viewport details to the app.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

/**
 * Reads the viewport configuration and reports it back to the browser.
 */
function reportViewportConfiguration(): void {
  // TODO(crbug.com/1394631): Find the current value of viewport-fit and report
  // it to the browser.
  sendWebKitMessage('FullscreenViewportHandler',
    {
      'cover' : false
    });
}

window.addEventListener('load', reportViewportConfiguration);
