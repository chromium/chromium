// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CSS_CLASS_NAME_SELECT}
    from '//ios/web/find_in_page/resources/find_in_page_constants.js';

/**
 * Returns the width of the document.body.  Sometimes though the body lies to
 * try to make the page not break rails, so attempt to find those as well.
 * An example: wikipedia pages for the ipad.
 * @return {number} Width of the document body.
 */
function getBodyWidth_(): number {
  let body = document.body;
  let documentElement = document.documentElement;
  return Math.max(
      body.scrollWidth, documentElement.scrollWidth, body.offsetWidth,
      documentElement.offsetWidth, body.clientWidth,
      documentElement.clientWidth);
};

/**
 * Returns the height of the document.body.  Sometimes though the body lies to
 * try to make the page not break rails, so attempt to find those as well.
 * An example: wikipedia pages for the ipad.
 * @return {number} Height of the document body.
 */
function getBodyHeight_(): number {
  let body = document.body;
  let documentElement = document.documentElement;
  return Math.max(
      body.scrollHeight, documentElement.scrollHeight, body.offsetHeight,
      documentElement.offsetHeight, body.clientHeight,
      documentElement.clientHeight);
};

/**
 * Helper function that determines if an element is visible.
 * @param {Element} elem Element to check.
 * @return {boolean} Whether elem is visible or not.
 */
function isElementVisible_(elem: HTMLElement): boolean {
  if (!elem) {
    return false;
  }
  let top = 0;
  let left = 0;
  let bottom = Infinity;
  let right = Infinity;

  let originalElement = elem;
  let nextOffsetParent = originalElement.offsetParent;

  // We are currently handling all scrolling through the app, which means we can
  // only scroll the window, not any scrollable containers in the DOM itself. So
  // for now this function returns false if the element is scrolled outside the
  // viewable area of its ancestors.
  // TODO(crbug.com/40606656): handle scrolling within the DOM.
  let bodyHeight = getBodyHeight_();
  let bodyWidth = getBodyWidth_();

  while (elem && elem.nodeName.toUpperCase() !== 'BODY') {
    if (elem.style.display === 'none' || elem.style.visibility === 'hidden') {
      return false;
    }

    // Check that there is a value set before converting to a Number, otherwise
    // and empty string will convert to opacity zero and a visible item will be
    // assumed hidden.
    if (elem.style.opacity.length) {
      const opacity = Number(elem.style.opacity);
      if (!isNaN(opacity) && opacity === 0) {
        return false;
      }
    }

    if (elem.ownerDocument && elem.ownerDocument.defaultView) {
      const computedStyle =
          elem.ownerDocument.defaultView.getComputedStyle(elem, null);
      if (computedStyle.display === 'none' ||
          computedStyle.visibility === 'hidden') {
        return false;
      }

      // Check that there is a value set before converting to a Number,
      // otherwise and empty string will convert to opacity zero and a visible
      // item will be assumed hidden.
      if (computedStyle.opacity.length) {
        const opacity = Number(computedStyle.opacity);
        if (!isNaN(opacity) && opacity === 0) {
          return false;
        }
      }
    }

    // For the original element and all ancestor offsetParents, trim down the
    // visible area of the original element.
    if (elem.isSameNode(originalElement) || elem.isSameNode(nextOffsetParent)) {
      let visible = elem.getBoundingClientRect();
      if (elem.style.overflow === 'hidden' &&
          (visible.width === 0 || visible.height === 0))
        return false;

      top = Math.max(top, visible.top + window.pageYOffset);
      bottom = Math.min(bottom, visible.bottom + window.pageYOffset);
      left = Math.max(left, visible.left + window.pageXOffset);
      right = Math.min(right, visible.right + window.pageXOffset);

      // The element is not within the original viewport.
      let notWithinViewport = top < 0 || left < 0;

      // The element is flowing off the boundary of the page. Note this is
      // not comparing to the size of the window, but the calculated offset
      // size of the document body. This can happen if the element is within
      // a scrollable container in the page.
      let offPage = right > bodyWidth || bottom > bodyHeight;
      if (notWithinViewport || offPage) {
        return false;
      }
      nextOffsetParent = elem.offsetParent;
    }

    if (!(elem.parentNode instanceof HTMLElement)) {
      break;
    }

    elem = elem.parentNode;
  }
  return true;
};


/**
 * A Match represents a match result in the document. |this.nodes| stores all
 * the <chrome_find> Nodes created for highlighting the matched text. If it
 * contains only one Node, it means the match is found within one HTML TEXT
 * Node, otherwise the match involves multiple HTML TEXT Nodes.
 */
class Match {
  nodes: HTMLElement[] = [];

  /**
   * Returns if all <chrome_find> Nodes of this match are visible.
   * @return {Boolean} If the Match is visible.
   */
  visible(): boolean {
    for (const node of this.nodes) {
      if (!isElementVisible_(node))
        return false;
    }
    return true;
  }

  /**
   * Adds orange color highlight for "selected match result", over the yellow
   * color highlight for "normal match result".
   */
  addSelectHighlight(): void {
    for (const node of this.nodes) {
      node.classList.add(CSS_CLASS_NAME_SELECT);
    }
  }

  /**
   * Clears the orange color highlight.
   */
  removeSelectHighlight(): void {
    for (const node of this.nodes) {
      node.classList.remove(CSS_CLASS_NAME_SELECT);
    }
  }
}

/**
 * A part of a Match, within a Section. A Match may cover multiple sections_ in
 * |allText_|, so it must be split into multiple PartialMatches and then
 * dispatched into the Sections they belong. The range of a PartialMatch in
 * |allText_| is [begin, end). Exactly one <chrome_find> will be created for
 * each PartialMatch.
 */
class PartialMatch {
  /**
   * @param {number} matchId ID of the Match to which this PartialMatch belongs.
   * @param {number} begin Beginning index of partial match text in |allText_|.
   * @param {number} end Ending index of partial match text in |allText_|.
   */
  constructor(
      public matchId: number, public begin: number, public end: number) {}
}

/**
 * A Replacement represents a DOM operation that swaps |oldNode| with |newNodes|
 * under the parent of |oldNode| to highlight the match result inside |oldNode|.
 * |newNodes| may contain plain TEXT Nodes for unhighlighted parts and
 * <chrome_find> nodes for highlighted parts. This operation will be executed
 * reversely when clearing current highlights for next FindInPage action.
 */
class Replacement {
  /**
   * @param {Node} HTMLElement The HTML Node containing search result.
   * @param {Array<Node>} newNodes New HTML Nodes created for
   *     substitution of |oldNode|.
   */
  constructor(
      private readonly oldNode: HTMLElement,
      private readonly newNodes: Node[]) {}

  /**
   * Executes the replacement to highlight search result.
   */
  doSwap(): void {
    let parentNode = this.oldNode.parentNode;
    if (!parentNode)
      return;
    for (const newNode of this.newNodes) {
      parentNode.insertBefore(newNode, this.oldNode);
    }
    parentNode.removeChild(this.oldNode);
  }

  /**
   * Executes the replacement reversely to clear the highlight.
   */
  undoSwap(): void {
    const firstNewNode = this.newNodes[0];
    if (!firstNewNode) {
      return;
    }
    let parentNode = firstNewNode.parentNode;
    if (!parentNode)
      return;
    parentNode.insertBefore(this.oldNode, firstNewNode);
    for (const newNode of this.newNodes) {
      parentNode.removeChild(newNode);
    }
  }
}

/**
 * A Section contains the info of one TEXT node in the |allText_|. The node's
 * textContent is [begin, end) of |allText_|.
 */
class Section {
  /**
   * @param {number} begin Beginning index of |node|.textContent in |allText_|.
   * @param {number} end Ending index of |node|.textContent in |allText_|.
   * @param {HTMLElement} node The TEXT Node of this section.
   */
  constructor(
      public begin: number, public end: number, public node: HTMLElement) {}
}

/**
 * A timer that checks timeout for long tasks.
 */
class Timer {
  private beginTime = Date.now();

  /**
   * @param {Number} timeoutMs Timeout in milliseconds.
   */
  constructor(private timeoutMs: number) {}

  /**
   * @return {Boolean} Whether this timer has been reached.
   */
  overtime(): boolean {
    return Date.now() - this.beginTime > this.timeoutMs;
  }
}

export {Match, PartialMatch, Replacement, Section, Timer}
