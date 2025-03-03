// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class name of CSS element that highlights matches with yellow.
 */
export const CSS_CLASS_NAME: string = 'find_in_page';

/**
 * Class name of CSS element that selects a highlighted match with orange.
 */
export const CSS_CLASS_NAME_SELECT: string = 'find_selected';

/**
 * ID of CSS style.
 */
export const CSS_STYLE_ID: string = '__gCrWeb.findInPageStyle';

/**
 * Node names that are not going to be processed.
 */
export const IGNORE_NODE_NAMES: Set<string> = new Set([
  'SCRIPT',
  'STYLE',
  'EMBED',
  'OBJECT',
  'SELECT',
  'TEXTAREA',
  'IFRAME',
  'NOSCRIPT',
]);

/**
 * Maximum number of visible elements to count
 */
export const MAX_VISIBLE_ELEMENTS: number = 100;

/**
 * Result passed back to app to indicate pumpSearch has reached timeout.
 */
export const TIMEOUT: number = -1;
