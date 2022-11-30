// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tool to fetch the surrounding text around a long
 * pressed character.
 */

// The number of characters to add after a long press going left or going right.
var NUMBER_OF_SURROUNDING_CHARS = 100;

// List of nodes whose contents should not be considered when extracting text.
var INVALID_TEXT_ELEMENTS = new Set([
  'APPLET',   'AREA',     'AUDIO',    'BUTTON', 'CANVAS',   'EMBED',
  'FRAME',    'FRAMESET', 'IFRAME',   'IMG',    'INPUT',    'KEYGEN',
  'LABEL',    'MAP',      'NOSCRIPT', 'OBJECT', 'OPTGROUP', 'OPTION',
  'PROGRESS', 'SCRIPT',   'SELECT',   'STYLE',  'TEXTAREA', 'VIDEO'
]);

// Mark: Private helper functions

/**
 * Returns the text of a given range adding a space between the contents of
 * adjacent nodes.
 * @param {range} a given range.
 * @return {string} a string where the text in the input range is joined with a
 *     space between the contents of adjacent nodes.
 */
var rangeToString = function(range) {
  if (range.startContainer === range.endContainer) {
    return range.startContainer.textContent.substring(
        range.startOffset, range.endOffset);
  }
  var node = range.startContainer;
  var text = node.textContent.substring(range.startOffset);
  node = getNextTextNode(node);
  while (node != range.endContainer) {
    text += ' ' + node.textContent;
    node = getNextTextNode(node);
  }
  text += ' ' + node.textContent.substring(0, range.endOffset);
  return text;
};

/**
 * Returns whether the a given element is valid.
 * An invalid selection is a selection that is either contains invalid or
 * contenteditable elements.
 * @param {element} The element to be checked.
 * @return {boolean} The element's validity.
 */
var isValidElement = function(element) {
  if (element.getAttribute('contenteditable')) {
    return false;
  }
  return !INVALID_TEXT_ELEMENTS.has(element.nodeName);
};

/**
 * Returns the last node that is a descendant of |node| and not a descendant of
 * an invalid node (DFS order).
 * @param {node} The current node.
 * @return {node} The last valid node.
 */
var getLastValid = function(node) {
  const childrenCount = node.childNodes.length;
  for (let i = childrenCount - 1; i >= 0; i--) {
    var child = node.childNodes[i];
    if ((child.nodeType == child.ELEMENT_NODE) && isValidElement(child)) {
      return getLastValid(child);
    }
    if (child.nodeType == child.TEXT_NODE) {
      return child;
    }
  }
  return node;
};

/**
 * Returns the previous valid text node.
 * @param {node} The current node.
 * @return {node} The previous valid text node.
 */
var getPrevNode = function(node) {
  if (!node || node == document.body) {
    return null;
  }

  while (node != null) {
    if (node.previousSibling) {
      node = node.previousSibling;
      if (node.nodeType == node.TEXT_NODE) {
        return node;
      }
      if (node.nodeType == node.ELEMENT_NODE && isValidElement(node)) {
        return getLastValid(node);
      }
      continue;
    }
    node = node.parentNode;
    if (node && isValidElement(node)) {
      return node;
    }
  }
  return null;
};

/**
 * Returns the next valid node.
 * @param {node} The current node.
 * @return {node} The next valid node.
 */
var getNextNode = function(node) {
  if (!node) {
    return null;
  }

  if (node.childNodes.length > 0) {
    node = node.childNodes[0];
    if (node.nodeType == node.TEXT_NODE ||
        (node.nodeType == node.ELEMENT_NODE && isValidElement(node))) {
      return node;
    }
    if (node.nodeType == node.ELEMENT_NODE) {
      return null;
    }
  }

  while (node != null) {
    if (!node.nextSibling) {
      node = node.parentNode;
      if (node == document.body) {
        return null;
      }
      continue;
    }
    node = node.nextSibling;
    if (node.nodeType == node.TEXT_NODE ||
        (node.nodeType == node.ELEMENT_NODE && isValidElement(node))) {
      return node;
    }
  }
  return null;
};

/**
 * Returns the next valid text node.
 * @param {node} The current node.
 * @return {node} The next valid text node.
 */
var getNextTextNode = function(node) {
  var n = getNextNode(node);
  while (n != null && n.nodeType != n.TEXT_NODE) {
    n = getNextNode(n);
  }
  return n;
};

/**
 * Returns the previous valid text node.
 * @param {node} The current node.
 * @return {node} The previous valid text node.
 */
var getPrevTextNode = function(node) {
  var n = getPrevNode(node);
  while (n != null && n.nodeType != n.TEXT_NODE) {
    n = getPrevNode(n);
  }
  return n;
};

// Mark: Public API functions called from native code.

/**
 * Returns an object representing the position of the selected point in text
 * within the surrounding text, and the surrounding range.
 * @param {range} the range where the user's selected point.
 * @return {!Object} An object of the form {
 *                     {@code pos} The selected text relative position.
 *                     {@code range} The new extended range.
 *                   }.
 */
function getSurroundingText(range) {
  window.testlog = '';
  var extendedRange = range.cloneRange();
  var leftCharacters = NUMBER_OF_SURROUNDING_CHARS;
  var rightCharacters = NUMBER_OF_SURROUNDING_CHARS;
  var leftNode = extendedRange.startContainer;
  var rightNode = extendedRange.endContainer;
  var pos = 0;

  if (extendedRange.startOffset >= NUMBER_OF_SURROUNDING_CHARS) {
    extendedRange.setStart(
        extendedRange.startContainer,
        extendedRange.startOffset - NUMBER_OF_SURROUNDING_CHARS);
  } else {
    let tmpPos = 0;
    leftCharacters -= extendedRange.startOffset;
    tmpPos += extendedRange.startOffset;
    while (leftCharacters > 0) {
      let tmpLeftNode = getPrevTextNode(leftNode);
      if (tmpLeftNode == null) {
        break;
      }
      leftNode = tmpLeftNode;
      leftCharacters -= leftNode.length;
      tmpPos += leftNode.length + 1;
    }
    extendedRange.setStart(leftNode, leftCharacters < 0 ? -leftCharacters : 0);
    pos = tmpPos + (leftCharacters < 0 ? leftCharacters : 0)
  }

  if (extendedRange.endContainer.length - extendedRange.endOffset >=
      NUMBER_OF_SURROUNDING_CHARS) {
    extendedRange.setEnd(
        extendedRange.endContainer,
        extendedRange.endOffset + NUMBER_OF_SURROUNDING_CHARS);
  } else {
    rightCharacters -=
        extendedRange.endContainer.length - extendedRange.endOffset;
    while (rightCharacters > 0) {
      let tmpRightNode = getNextTextNode(rightNode);
      if (tmpRightNode == null) {
        break;
      }
      rightNode = tmpRightNode;
      rightCharacters -= rightNode.length;
    }
    extendedRange.setEnd(
        rightNode,
        rightCharacters < 0 ? rightNode.length + rightCharacters :
                              rightNode.length);
  }
  return {'pos': pos, 'text': rangeToString(extendedRange)};
};

export {getSurroundingText}
