// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to font size adjustment.
 */
goog.provide('__crWeb.font_size');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.font_size = {};

/**
 * Store common namespace object in a global __gCrWeb object referenced by a
 * string, so it does not get renamed by closure compiler during the
 * minification.
 */
__gCrWeb['font_size'] = __gCrWeb.font_size;

/**
 * Adjust the font size of current web page by "size%"
 *
 * TODO(crbug.com/836962): Consider the original value of
 *                         -webkit-text-size-adjust on the web page. Add it
 *                         if it's a number or abort if it's 'none'.
 *
 * @param {number} size The ratio to apply to font scaling in %.
 */
__gCrWeb.font_size.adjustFontSize = function(size) {
  try {
    document.body.style.webkitTextSizeAdjust = size + '%';
  } catch (error) {
  }
};
