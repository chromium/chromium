// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DOM related utilities.
 */

// Semantically extends `HTMLElement` to allow using `Symbol` as property index.
class HTMLElementWithSymbolIndex extends HTMLElement {
  [key: symbol|string]: any
}

// Semantically extends `Element` to allow using `Symbol` as property index.
class ElementWithSymbolIndex extends HTMLElement {
  [key: symbol|string]: any
}

// Semantically extends `Node` to allow using `Symbol` as property index.
class NodeWithSymbolIndex extends Node {
  [key: symbol|string]: any
}

// Semantically extends `Text` to allow using `Symbol` as property index.
class TextWithSymbolIndex extends Text {
  [key: symbol|string]: any
}

// Interface for exportable part of a `DOMRect` (`x`, `y`, `width` and
// `height`).
interface Rect {
  height?: number;
  width?: number;
  x?: number;
  y?: number;
}

// Note that 'CHROME_ANNOTATION' is here to avoid handling decoration twice.
// FORM is here to avoid messing anything inside a form, like email addresses.
const IGNORE_NODE_NAMES = new Set([
  'SCRIPT',   'NOSCRIPT', 'STYLE',    'EMBED',    'OBJECT',
  'TEXTAREA', 'IFRAME',   'INPUT',    'IMG',      'CHROME_ANNOTATION',
  'HEAD',     'APPLET',   'AREA',     'AUDIO',    'BUTTON',
  'CANVAS',   'FRAME',    'FRAMESET', 'KEYGEN',   'LABEL',
  'MAP',      'OPTGROUP', 'OPTION',   'PROGRESS', 'SELECT',
  'VIDEO',    'A',        'APP',      'FORM',     'SVG',
]);

// Gets the content of a meta tag by httpEquiv for `httpEquiv`. The function is
// case insensitive.
function getMetaContentByHttpEquiv(httpEquiv: string): string {
  const metas = document.getElementsByTagName('meta');
  for (const meta of metas) {
    if (meta.httpEquiv.toLowerCase() === httpEquiv) {
      return meta.content;
    }
  }
  return '';
}

// Returns all types in meta tags 'format-detection', where the type is
// assigned 'no'.
function noFormatDetectionTypes(): Set<string> {
  const metas = document.getElementsByTagName('meta');
  let types = new Set<string>();
  for (const meta of metas) {
    if (meta.getAttribute('name') !== 'format-detection')
      continue;
    let content = meta.getAttribute('content');
    if (!content)
      continue;
    let matches = content.toLowerCase().matchAll(/([a-z]+)\s*=\s*([a-z]+)/gi);
    if (!matches)
      continue;
    for (let match of matches) {
      if (match && match[2] === 'no' && match[1]) {
        types.add(match[1]);
      }
    }
  }
  return types;
}

// Searches page elements for "nointentdetection" meta tag. Returns true if
// "nointentdetection" meta tag is defined.
function hasNoIntentDetection(): boolean {
  const metas = document.getElementsByTagName('meta');
  for (const meta of metas) {
    if (meta.getAttribute('name') === 'chrome' &&
        meta.getAttribute('content') === 'nointentdetection') {
      return true;
    }
  }
  return false;
}

// Returns whether an annotation of type `annotationType` can be across element.
function annotationCanBeCrossElement(annotationType: String) {
  return ['address', 'email'].includes(annotationType.toLowerCase());
}

// Returns the client rectangle for the given `element` {x, y, width, height}.
function rectFromElement(element: Element): Rect {
  const domRect = element.getClientRects()[0];
  if (!domRect) {
    // TODO(crbug.com/40936184): modify pipeline for returning null here and make
    // `Rect`'s x, y, width, height required.
    return {};
  }
  return {
    x: domRect.x,
    y: domRect.y,
    width: domRect.width,
    height: domRect.height
  };
}

// Returns whether the given node is valid. An invalid node is one in
// `IGNORE_NODE_NAMES` or if it is a contenteditable element.
function isValidNode(node: Node): boolean {
  if (node instanceof Element && node.getAttribute('contenteditable')) {
    return false;
  }
  return !IGNORE_NODE_NAMES.has(node.nodeName);
}

// Returns previous leaf `Node` in the DOM tree, starting from given `node`.
// If `breakAtInvalid` null will be return if an invalid node is found during
// traversal. Note the case where `node` is a descendant of an invalid node
// is not detected by this code. Note also that ShadowRoots are not followed.
function previousLeaf(node: Node|null, breakAtInvalid = false): Node|null {
  while (node) {
    // Find somewhere we can go left, by moving up the tree if needed.
    while (node && !node.previousSibling) {
      node = node.parentNode;
    }
    node = node && node.previousSibling;
    if (node && isValidNode(node)) {
      // Get rightmost node.
      while (node && isValidNode(node) && node.hasChildNodes()) {
        node = node.lastChild;
      }
      if (node && isValidNode(node)) {
        return node;
      }
    }
    if (breakAtInvalid) {
      return null;
    }
  }
  return null;
}

// Returns next leaf `Node` in the DOM tree, starting from given `node`.
// See `previousLeaf` for more information.
function nextLeaf(node: Node|null, breakAtInvalid = false): Node|null {
  while (node) {
    // Find somewhere we can go right, by moving up the tree if needed.
    while (node && !node.nextSibling) {
      node = node.parentNode;
    }
    node = node && node.nextSibling;
    if (node && isValidNode(node)) {
      // Get leftmost node.
      while (node && isValidNode(node) && node.hasChildNodes()) {
        node = node.firstChild;
      }
      if (node && isValidNode(node)) {
        return node;
      }
    }
    if (breakAtInvalid) {
      return null;
    }
  }
  return null;
}

export {
  HTMLElementWithSymbolIndex,
  ElementWithSymbolIndex,
  NodeWithSymbolIndex,
  TextWithSymbolIndex,
  Rect,
  annotationCanBeCrossElement,
  getMetaContentByHttpEquiv,
  noFormatDetectionTypes,
  hasNoIntentDetection,
  rectFromElement,
  isValidNode,
  previousLeaf,
  nextLeaf,
}
