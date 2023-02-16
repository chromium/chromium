// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Interface used for Chrome to retrieve the web page selection
 * and its bounding box.
 */

/**
 * Retrieves the current page text selection, if any.
 */
function getSelectedText() {
  const selection = window.getSelection();
  let selectionRect = {x: 0, y: 0, width: 0, height: 0};

  if (selection && selection.rangeCount) {
    // Get the selection range's first client rect.
    const domRect = selection.getRangeAt(0).getClientRects()[0];
    if (domRect) {
      selectionRect.x = domRect.x;
      selectionRect.y = domRect.y;
      selectionRect.width = domRect.width;
      selectionRect.height = domRect.height;
    }
  }

  const selectedText = `${selection?.toString()}`;

  return {
    selectedText: selectedText,
    selectionRect: selectionRect,
  };
}

gCrWeb.webSelection =  { getSelectedText };
