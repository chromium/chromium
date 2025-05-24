// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Add functionality related to font size adjustment.
 */

/**
 * This interface extends the CSSStyleDeclaration interface
 * to include the webkitTextSizeAdjust property. Text-size-adjust property
 * is experimential, so it is not included in the CSSStyleDeclaration
 * object.
 */
interface CssTextSizeAdjust extends CSSStyleDeclaration {
  webkitTextSizeAdjust: string;
}

/**
 * Adjust the font size of current web page by "size%"
 *
 * TODO(crbug.com/41385551): Consider the original value of
 *                         -webkit-text-size-adjust on the web page. Add it
 *                         if it's a number or abort if it's 'none'.
 *
 * @param {number} size The ratio to apply to font scaling in %.
 */
function adjustFontSize(size: number): void {
  try {
    (document.body.style as CssTextSizeAdjust).webkitTextSizeAdjust =
        `${size}%`;
  } catch (error) {}
}

gCrWebLegacy.font_size = {adjustFontSize};
