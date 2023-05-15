// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tool to fetch the surrounding text around a long
 * pressed character.
 */

// The number of characters to add after a long press going left or going right.
const NUMBER_OF_SURROUNDING_CHARS = 100;

// List of nodes whose contents should not be considered when extracting text.
const INVALID_TEXT_ELEMENTS = new Set([
  'APPLET',   'AREA',     'AUDIO',    'BUTTON', 'CANVAS',   'EMBED',
  'FRAME',    'FRAMESET', 'IFRAME',   'IMG',    'INPUT',    'KEYGEN',
  'LABEL',    'MAP',      'NOSCRIPT', 'OBJECT', 'OPTGROUP', 'OPTION',
  'PROGRESS', 'SCRIPT',   'SELECT',   'STYLE',  'TEXTAREA', 'VIDEO'
]);

/**
 * Contains `position`, as an index of the selected text and `text` the
 * surrounding text extended by `NUMBER_OF_SURROUNDING_CHARS` before and after
 * as much as possible.
 */
class SurroundingText {
  constructor(public position: number, public text: string) {}
}

// Mark: Private helper functions

/**
 * Returns whether the given element is valid.
 * An invalid element is one in `INVALID_TEXT_ELEMENTS` or if it is a
 * contenteditable element.
 */
function isValidElement(element: Element): boolean {
  if (element.getAttribute('contenteditable')) {
    return false;
  }
  return !INVALID_TEXT_ELEMENTS.has(element.nodeName);
}

/**
 * Returns the last node that is a descendant of `node` and not a descendant of
 * an invalid node (DFS order).
 */
function getLastValid(node: Node): Node {
  const childrenCount = node.childNodes.length;
  for (let i = childrenCount - 1; i >= 0; i--) {
    const child = node.childNodes[i];
    if (!child)
      continue;
    if (child instanceof Element && isValidElement(child)) {
      return getLastValid(child);
    }
    if (child.nodeType === child.TEXT_NODE) {
      return child;
    }
  }
  return node;
}

/**
 * Returns the previous valid text node.
 */
function getPrevNode(node: Node|null): Node|null {
  if (!node || node === document.body) {
    return null;
  }

  while (node != null) {
    if (node.previousSibling) {
      node = node.previousSibling;
      if (node.nodeType === node.TEXT_NODE) {
        return node;
      }
      if (node instanceof Element && isValidElement(node)) {
        return getLastValid(node);
      }
      continue;
    }
    node = node.parentNode;
    if (node instanceof Element && isValidElement(node)) {
      return node;
    }
  }
  return null;
}

/**
 * Returns the next valid node.
 */
function getNextNode(node: Node|null): Node|null {
  if (!node) {
    return null;
  }

  if (node.childNodes.length > 0) {
    node = node.childNodes[0]!;
    if (node.nodeType === node.TEXT_NODE ||
        (node instanceof Element && isValidElement(node))) {
      return node;
    }
    if (node.nodeType === node.ELEMENT_NODE) {
      return null;
    }
  }

  while (node != null) {
    if (!node.nextSibling) {
      node = node.parentNode;
      if (node === document.body) {
        return null;
      }
      continue;
    }
    node = node.nextSibling;
    if (node.nodeType === node.TEXT_NODE ||
        (node instanceof Element && isValidElement(node))) {
      return node;
    }
  }
  return null;
}

/**
 * Returns the next valid text node.
 */
function getNextTextNode(node: Node|null): Node|null {
  let n = getNextNode(node);
  while (n != null && n.nodeType != n.TEXT_NODE) {
    n = getNextNode(n);
  }
  return n;
}

/**
 * Returns the previous valid text node.
 */
function getPrevTextNode(node: Node|null): Node|null {
  var n = getPrevNode(node);
  while (n != null && n.nodeType != n.TEXT_NODE) {
    n = getPrevNode(n);
  }
  return n;
}

// Mark: Public API functions called from native code.

/**
 * Returns an object representing the position of the selected point in text
 * within the surrounding text, and the surrounding range.
 * @param range - the range where the user's selected point.
 */
function getSurroundingText(range: Range): SurroundingText {
  const node = range.startContainer;
  const textContent = node.textContent;
  if (!textContent) {
    return new SurroundingText(/*position=*/ 0, /*text=*/ '');
  }

  let leftText = textContent.substring(0, range.startOffset);
  let leftNode = getPrevTextNode(node);
  while (leftNode && leftText.length < NUMBER_OF_SURROUNDING_CHARS) {
    leftText = leftNode.textContent + ' ' + leftText;
    leftNode = getPrevTextNode(leftNode);
  }

  let rightText = textContent.substring(range.endOffset);
  let rightNode = getNextTextNode(node);
  while (rightNode && rightText.length < NUMBER_OF_SURROUNDING_CHARS) {
    rightText = rightText + ' ' + rightNode.textContent;
    rightNode = getNextTextNode(rightNode);
  }

  if (leftText.length > NUMBER_OF_SURROUNDING_CHARS) {
    leftText =
        leftText.substring(leftText.length - NUMBER_OF_SURROUNDING_CHARS);
  }
  if (rightText.length > NUMBER_OF_SURROUNDING_CHARS) {
    rightText = rightText.substring(0, NUMBER_OF_SURROUNDING_CHARS);
  }

  const middleText = textContent.substring(range.startOffset, range.endOffset);
  return new SurroundingText(
      leftText.length, leftText + middleText + rightText);
};

export {getSurroundingText, SurroundingText}
