// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import * as utils from '//third_party/text-fragments-polyfill/src/src/fragment-generation-utils.js';

/**
 * @fileoverview Interface used for Chrome to use link-to-text link generation
 * the utility functions from the text-fragments-polyfill library.
 */

/**
 * Attempts to generate a link with text fragments for the current
 * page's selection.
 */
function getLinkToText() {
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

  const selectedText = `"${selection?.toString()}"`;
  const canonicalLinkNode = document.querySelector('link[rel=\'canonical\']');

  const response = utils.generateFragment(selection as Selection);
  return {
    status: response.status,
    fragment: response.fragment,
    selectedText: selectedText,
    selectionRect: selectionRect,
    canonicalUrl: canonicalLinkNode
        && canonicalLinkNode.getAttribute('href')
  };
}

gCrWeb.linkToText =  { getLinkToText };
