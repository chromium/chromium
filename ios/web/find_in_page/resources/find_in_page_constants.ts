// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class name of CSS element that highlights matches with yellow.
 * @type {string}
 */
const CSS_CLASS_NAME = 'find_in_page';

/**
 * Class name of CSS element that selects a highlighted match with orange.
 * @type {string}
 */
const CSS_CLASS_NAME_SELECT = 'find_selected';

/**
 * ID of CSS style.
 * @type {string}
 */
const CSS_STYLE_ID = '__gCrWeb.findInPageStyle';

/**
 * Node names that are not going to be processed.
 * @type {Object}
 */
const IGNORE_NODE_NAMES = new Set([
  'SCRIPT', 'STYLE', 'EMBED', 'OBJECT', 'SELECT', 'TEXTAREA', 'IFRAME',
  'NOSCRIPT'
]);

/**
 * Maximum number of visible elements to count
 * @type {number}
 */
const MAX_VISIBLE_ELEMENTS = 100;

/**
 * Result passed back to app to indicate pumpSearch has reached timeout.
 * @type {number}
 */
const TIMEOUT = -1;

export {
  CSS_CLASS_NAME,
  CSS_CLASS_NAME_SELECT,
  CSS_STYLE_ID,
  IGNORE_NODE_NAMES,
  MAX_VISIBLE_ELEMENTS,
  TIMEOUT
}
