// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defensive DOM property and method accessors that guard against
 * DOM clobbering (e.g. form elements with child inputs named after standard DOM
 * properties or methods).
 */

const nodeTypeGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'nodeType')?.get;
const parentElementGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'parentElement')?.get;
const parentNodeGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'parentNode')?.get;
const isConnectedGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'isConnected')?.get;
const tagNameGetter =
    Object.getOwnPropertyDescriptor(Element.prototype, 'tagName')?.get;
const hasAttributeMethod = Element.prototype.hasAttribute;
const getAttributeMethod = Element.prototype.getAttribute;

/**
 * Returns the `nodeType` of a `Node` using the prototype getter.
 * @param node The DOM node.
 * @return The node type integer.
 */
export function safeNodeType(node: Node): number {
  return nodeTypeGetter ? nodeTypeGetter.call(node) : node.nodeType;
}

/**
 * Returns the `parentElement` of a `Node` using the prototype getter.
 * @param node The DOM node.
 * @return The parent element, or null if detached or parent is not an Element.
 */
export function safeParentElement(node: Node): Element|null {
  return parentElementGetter ? parentElementGetter.call(node) :
                               (node as Element).parentElement;
}

/**
 * Returns the `parentNode` of a `Node` using the prototype getter.
 * @param node The DOM node.
 * @return The parent node, or null if detached.
 */
export function safeParentNode(node: Node): Node|null {
  return parentNodeGetter ? parentNodeGetter.call(node) : node.parentNode;
}

/**
 * Invokes `Element.prototype.hasAttribute` safely on the element.
 * @param element The DOM element.
 * @param name The attribute name.
 * @return True if the element has the specified attribute.
 */
export function safeHasAttribute(element: Element, name: string): boolean {
  return hasAttributeMethod.call(element, name);
}

/**
 * Invokes `Element.prototype.getAttribute` safely on the element.
 * @param element The DOM element.
 * @param name The attribute name.
 * @return The attribute value, or null if not present.
 */
export function safeGetAttribute(element: Element, name: string): string|null {
  return getAttributeMethod.call(element, name);
}

/**
 * Returns the `tagName` of an `Element` using the prototype getter.
 * Safe against DOM clobbering (e.g. `<form><input name="tagName"></form>`).
 * @param element The DOM element.
 * @return The tag name in uppercase string.
 */
export function safeTagName(element: Element): string {
  return tagNameGetter ? tagNameGetter.call(element) : element.tagName;
}

/**
 * Returns whether a `Node` is connected to the DOM using the prototype getter.
 * Safe against DOM clobbering (e.g. `<form><input name="isConnected"></form>`).
 * @param node The DOM node.
 * @return True if connected to the document.
 */
export function safeIsConnected(node: Node): boolean {
  return isConnectedGetter ? isConnectedGetter.call(node) : node.isConnected;
}

/**
 * Returns true if both elements (or nodes) remain connected to the document.
 * Safe against DOM clobbering (e.g. `<form><input name="isConnected"></form>`).
 * @param a The first DOM node or element.
 * @param b The second DOM node or element.
 * @return True if both nodes are connected to the document.
 */
export function areElementsConnected(a: Node, b: Node): boolean {
  return safeIsConnected(a) && safeIsConnected(b);
}

/**
 * Returns whether an element is an HTML5 draggable candidate.
 * Safe against DOM clobbering on `draggable`, `tagName`, and `href`.
 * @param element The DOM element.
 * @return True if the element is an HTML5 draggable target.
 */
export function safeIsDraggable(element: Element): boolean {
  const draggable = safeGetAttribute(element, 'draggable');
  if (draggable === 'false') {
    return false;
  } else if (draggable === 'true') {
    return true;
  } else {
    // Fallback to browser default behavior: images and links are draggable.
    const tag = safeTagName(element).toLowerCase();
    if (tag === 'img') {
      return true;
    }
    if (tag === 'a' && safeHasAttribute(element, 'href')) {
      return true;
    }
  }
  return false;
}
