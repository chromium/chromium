// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test helpers for the annotations manager.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {NON_TEXT_NODE_NAMES}
    from '//ios/web/annotations/resources/annotations_constants.js';

// Simpleton tags with no closing tags (only those used in tests).
const NO_END_TAGS_NODE_NAMES = new Set([
  'br'
]);

/**
 * Returns count of <chrome_annotation>s.
 */
function countAnnotations(): number {
  let nodes = document.querySelectorAll("chrome_annotation");
  return nodes.length;
}

/**
 * Simulate clicking annotation at given `index`.
 */
function clickAnnotation(index: number, viewport: boolean): boolean {
  let nodes = document.querySelectorAll("chrome_annotation");
  const decoration = nodes[index];
  if (decoration && decoration instanceof HTMLElement) {
    if (viewport) {
      const rect = decoration.getBoundingClientRect();
      const event = new MouseEvent('click', {
        bubbles: true,
        cancelable: true,
        clientX: rect.x + 1,
        clientY: rect.y + 1
      });
      decoration.parentElement!.dispatchEvent(event);
    } else {
      decoration.click();
    }
    return true;
  }
  return false;
}

/**
 * Returns first `maxChars` characters from the page text and tags.
 * @param maxChars - maximum number of characters to parse out.
 */
 function getPageTaggedText(maxChars: number): string {
  const parts: string[] = [];
  let length = 0;
  function traverse(node: Node) {
    if (length >= maxChars) return;
    if (node.nodeType === Node.ELEMENT_NODE) {
      // Reject non-text nodes such as scripts.
      if (NON_TEXT_NODE_NAMES.has(node.nodeName) &&
          node.nodeName !== 'CHROME_ANNOTATION') {
        return;
      }
      const element = node as Element;
      if (element.shadowRoot && element.shadowRoot != node) {
        traverse(element.shadowRoot);
        return;
      }

      let tagName = element.tagName.toLowerCase();
      parts.push('<' + tagName + '>');
      length += tagName.length + 2;
      if (node.hasChildNodes()) {
        for (let child of node.childNodes) {
          traverse(child);
        }
      }
      if (!NO_END_TAGS_NODE_NAMES.has(tagName)) {
        parts.push('</' + tagName + '>');
        length += tagName.length + 2;
      }
    } else if (node.nodeType === Node.TEXT_NODE && node.textContent) {
      parts.push(node.textContent);
      length += node.textContent.length;
    }
  }
  parts.push('<html>');
  traverse(document.body);
  parts.push('</html>');
  return ''.concat(...parts);
}

gCrWeb.annotationsTest = {
  getPageTaggedText,
  countAnnotations,
  clickAnnotation,
};
