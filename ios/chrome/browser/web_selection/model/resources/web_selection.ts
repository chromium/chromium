// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Interface used for Chrome to retrieve the web page selection
 * and its bounding box.
 */

/**
 * Retrieves the current page text selection, if any.
 * Helper function to call getSelectedTextWithOffset(0,0)
 */
function getSelectedText() {
  getSelectedTextWithOffset(0, 0);
}

/**
 * Get the selection in the current document or forward to subframes.
 * @param offsetx - The x offset of the current frame in the whole document.
 * @param offsety - The y offset of the current frame in the whole document.
 */
function getSelectedTextWithOffset(offsetX: number, offsetY: number) {
  const selection = window.getSelection();
  let selectionRect = {x: 0, y: 0, width: 0, height: 0};

  if (!selection || !selection.rangeCount) {
    const iframes = document.getElementsByTagName('iframe');
    for (var iframe of iframes) {
      const domRect = iframe.getBoundingClientRect();
      iframe.contentWindow?.postMessage(
          {
            type: 'org.chromium.getSelection',
            'offsetX': domRect.x,
            'offsetY': domRect.y
          },
          '*');
    }
    return;
  }

  // Get the selection range's first client rect.
  const domRect = selection.getRangeAt(0).getClientRects()[0];
  if (domRect) {
    selectionRect.x = domRect.x + offsetX;
    selectionRect.y = domRect.y + offsetY;
    selectionRect.width = domRect.width;
    selectionRect.height = domRect.height;
  }

  const selectedText = `${selection?.toString()}`;
  sendWebKitMessage('WebSelection', {
    'selectedText': selectedText,
    'selectionRect': selectionRect,
  });
}

gCrWeb.webSelection =  { getSelectedText };

window.addEventListener('message', function(message) {
  const payload = message.data;
  if (!payload ||
      typeof payload !== 'object' ||
      !payload.hasOwnProperty('type') ||
      payload.type !== 'org.chromium.getSelection' ||
      !payload.hasOwnProperty('offsetX') ||
      !payload.hasOwnProperty('offsetY')) {
    return;
  }
  const x = payload.offsetX;
  const y = payload.offsetY;
  getSelectedTextWithOffset(x, y);
});
