// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as utils from '//third_party/text-fragments-polyfill/src/src/fragment-generation-utils.js';

/**
 * @fileoverview Interface used for Chrome to use link-to-text link generation
 * the utility functions from the text-fragments-polyfill library.
 */

__gCrWeb['linkToText'] = {};

/**
 * Attempts to generate a link with text fragments for the current
 * page's selection.
 */
__gCrWeb.linkToText.getLinkToText =
    function() {
  const selection = window.getSelection();
  const selectedText = `"${selection.toString()}"`;
  const selectionRect = {x: 0, y: 0, width: 0, height: 0};

  if (selection.rangeCount) {
    // Get the selection range's first client rect.
    const domRect = selection.getRangeAt(0).getClientRects()[0];
    selectionRect.x = domRect.x;
    selectionRect.y = domRect.y;
    selectionRect.width = domRect.width;
    selectionRect.height = domRect.height;
  }

  const canonicalLinkNode = document.querySelector('link[rel=\'canonical\']');

  const response = utils.generateFragment(selection);
  return {
    status: response.status,
    fragment: response.fragment,
    selectedText: selectedText,
    selectionRect: selectionRect,
    canonicalUrl: canonicalLinkNode && canonicalLinkNode.getAttribute('href')
  };
}

/**
 * Checks if the range is suitable to attempt link generation; the feature
 * should be disabled if this does not return true.
 */
__gCrWeb.linkToText.checkPreconditions = function() {
  return utils.isValidRangeForFragmentGeneration(
      window.getSelection().getRangeAt(0));
}
