// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Regex to escape regex special characters in a string.
 * @type {RegExp}
 */
const REGEX_ESCAPER = /([.?*+^$[\]\\(){}|-])/g;

/**
 * Creates the regex needed to find the text.
 * @param {string} findText Phrase to look for.
 * @return {RegExp} regex needed to find the text.
 */
function createRegex(findText: string): RegExp {
  const escapedText = findText.replace(REGEX_ESCAPER, '\\$1');
  const regexString = '(' + escapedText + ')';
  return new RegExp(regexString, 'ig');
};

/**
 * @param {string} text Text to escape.
 * @return {string} escaped text.
 */
function escapeHTML(text: string): string {
  let unusedDiv = document.createElement('div');
  unusedDiv.innerText = text;
  return unusedDiv.innerHTML;
};

export {createRegex, escapeHTML}
