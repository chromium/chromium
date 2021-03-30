// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

goog.provide('__crWeb.mainFrameContextMenu');

// Requires __crWeb.allFramesContextMenu provided by __crWeb.allFramesWebBundle.

/** Beginning of anonymous object */
(function() {

/**
 * Finds the url of the image or link under the selected point. Sends the
 * found element (or an empty object if no links or images are found) back to
 * the application by posting a 'FindElementResultHandler' message.
 * The object returned in the message is of the same form as
 * {@code findElementAtPointInPageCoordinates} result.
 * @param {string} requestId An identifier which be returned in the result
 *                 dictionary of this request.
 * @param {number} x Horizontal center of the selected point in web view
 *                 coordinates.
 * @param {number} y Vertical center of the selected point in web view
 *                 coordinates.
 * @param {number} webViewWidth the width of web view.
 * @param {number} webViewHeight the height of web view.
 */
__gCrWeb['findElementAtPoint'] =
    function(requestId, x, y, webViewWidth, webViewHeight) {
      var scale = getPageWidth() / webViewWidth;
      __gCrWeb.findElementAtPointInPageCoordinates(requestId,
                                                   x * scale,
                                                   y * scale);
    };

/**
 * Returns the margin in points around touchable elements (e.g. links for
 * custom context menu).
 * @type {number}
 */
var getPageWidth = function() {
  var documentElement = document.documentElement;
  var documentBody = document.body;
  return Math.max(
      documentElement.clientWidth, documentElement.scrollWidth,
      documentElement.offsetWidth, documentBody.scrollWidth,
      documentBody.offsetWidth);
};

}());  // End of anonymouse object
