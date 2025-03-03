// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Regex to escape regex special characters in a string.
 */
const REGEX_ESCAPER: RegExp = /([.?*+^$[\]\\(){}|-])/g;

/**
 * Creates the regex needed to find the text.
 * @param findText Phrase to look for.
 * @return regex needed to find the text.
 */
export function createRegex(findText: string): RegExp {
  const escapedText = findText.replace(REGEX_ESCAPER, '\\$1');
  const regexString = '(' + escapedText + ')';
  return new RegExp(regexString, 'ig');
}

/**
 * @param text Text to escape.
 * @return escaped text.
 */
export function escapeHTML(text: string): string {
  const unusedDiv = document.createElement('div');
  unusedDiv.innerText = text;
  return unusedDiv.innerHTML;
}
