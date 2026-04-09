// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions to allow Chrome on iOS to scroll to or scroll
 * within an element.
 */

import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// 'center' is selected to align with Desktop's behavior in
// WebElement::ScrollIntoViewIfNeeded.
const SCROLL_INTO_VIEW_OPTIONS: ScrollIntoViewOptions = {
  // 'auto' behavior is used to avoid delays from using 'smooth'.
  behavior: 'auto',
  block: 'center',
  inline: 'center',
};

// Mirrored from
// components/optimization_guide/proto/features/actions_data.proto.
enum ScrollDirection {
  // Unknown scroll direction. This should not be used.
  UNKNOWN_SCROLL_DIRECTION = 0,
  // Scroll left.
  LEFT = 1,
  // Scroll right.
  RIGHT = 2,
  // Scroll up.
  UP = 3,
  // Scroll down.
  DOWN = 4,
}

type ScrollDirectionX = ScrollDirection.LEFT|ScrollDirection.RIGHT;
type ScrollDirectionY = ScrollDirection.UP|ScrollDirection.DOWN;

function getScrollDirection(direction: number): ScrollDirection {
  switch (direction) {
    case 1:
      return ScrollDirection.LEFT;
    case 2:
      return ScrollDirection.RIGHT;
    case 3:
      return ScrollDirection.UP;
    case 4:
      return ScrollDirection.DOWN;
    default:
      return ScrollDirection.UNKNOWN_SCROLL_DIRECTION;
  }
}

// Additional parameters for the ScrollAction that aren't needed for the
// ScrollToAction.
interface ScrollParams {
  distance: number;
  direction: ScrollDirection;
}

// Checks if an elements style allows scrolling.
function hasScrollableOverflowStyle(
    element: Element, overflowProperty: string): boolean {
  const overflowStyle =
      window.getComputedStyle(element).getPropertyValue(overflowProperty);
  if (overflowStyle === 'auto' || overflowStyle === 'scroll') {
    return true;
  }
  // `document.scrollingElement` may not have the overflow-* values set, in
  // which case the browser treats 'visible' like 'auto'. See
  // https://www.w3.org/TR/css-overflow-3/#overflow-propagation.
  const isDocumentScrollingElement = element === document.scrollingElement;
  return (isDocumentScrollingElement && overflowStyle === 'visible');
}

// Checks if an element is scrollable along the x-axis in the given direction.
function isScrollableX(element: Element, direction: ScrollDirectionX): boolean {
  if (!hasScrollableOverflowStyle(element, /*overflowProperty*/ 'overflow-x')) {
    return false;
  }
  const hasScrollableContentX = element.scrollWidth > element.clientWidth;
  if (!hasScrollableContentX) {
    return false;
  }
  switch (direction) {
    case ScrollDirection.LEFT:
      return element.scrollLeft > 0;
    case ScrollDirection.RIGHT:
      return Math.ceil(element.scrollLeft) + element.clientWidth <
          element.scrollWidth;
    default:
      return false;  // Unreachable
  }
}

// Checks if an element is scrollable along the y-axis in the given direction.
function isScrollableY(element: Element, direction: ScrollDirectionY): boolean {
  if (!hasScrollableOverflowStyle(element, /*overflowProperty*/ 'overflow-y')) {
    return false;
  }
  const hasScrollableContentY = element.scrollHeight > element.clientHeight;
  if (!hasScrollableContentY) {
    return false;
  }
  switch (direction) {
    case ScrollDirection.UP:
      return element.scrollTop > 0;
    case ScrollDirection.DOWN:
      return Math.ceil(element.scrollTop) + element.clientHeight <
          element.scrollHeight;
    default:
      return false;  // Unreachable
  }
}

/**
 * Checks if an element is scrollable in a given direction.
 * @param element The element to check.
 * @return true if the element is scrollable.
 */
export function isScrollable(
    element: Element, direction: ScrollDirection): boolean {
  switch (direction) {
    case ScrollDirection.LEFT:
      return isScrollableX(element, direction);
    case ScrollDirection.RIGHT:
      return isScrollableX(element, direction);
    case ScrollDirection.UP:
      return isScrollableY(element, direction);
    case ScrollDirection.DOWN:
      return isScrollableY(element, direction);
    default:
      return false;
  }
}

/**
 * Finds the first scrollable ancestor of an element in a given direction.
 *
 * This behavior is mirrored from ScrollTool::Validate on desktop linked below:
 * https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/scroll_tool.cc;l=134-173;drc=06d06050f1a98a6ce06cc1dc4470eebaf5a81990
 *
 * @param element The element to check.
 * @param direction The scroll direction.
 * @return The first scrollable ancestor, or null if none found.
 */
function findScrollableAncestor(
    element: Element, direction: ScrollDirection): Element|null {
  let current: Node|null = element as Node;
  while (current) {
    if (current.nodeType === Node.DOCUMENT_NODE) {
      const scrollingElement = document.scrollingElement;
      if (scrollingElement && isScrollable(scrollingElement, direction)) {
        return scrollingElement;
      }
    }

    if (current instanceof Element && isScrollable(current, direction)) {
      return current;
    }

    if (current instanceof ShadowRoot) {
      current = current.host;
    } else {
      current = current.parentNode;
    }
  }
  return null;
}

/**
 * Scrolls an element into view.
 * @param target The target element.
 * @return an object containing the result of the scroll attempt.
 */
function scrollElementIntoView(target: Element): {
  success: boolean,
  message: string,
} {
  target.scrollIntoView(SCROLL_INTO_VIEW_OPTIONS);
  return {success: true, message: 'Initiated scrollIntoView.'};
}

/**
 * Scrolls an element based on the params. If scrollParams omitted, the element
 * is scrolled into view.
 * @param target The target element.
 * @param [scrollParams] The scroll parameters.
 * @return an object containing the result of the scroll attempt.
 */
function scrollElement(target: Element, scrollParams?: ScrollParams):
    {success: boolean, message: string} {
  if (!scrollParams) {
    return scrollElementIntoView(target);
  }
  // The desktop ScrollTool first validates that the target is scrollable.
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/scroll_tool.cc;l=134-196;drc=06d06050f1a98a6ce06cc1dc4470eebaf5a81990
  if (!isScrollable(target, scrollParams.direction)) {
    return {success: false, message: 'Element is not scrollable.'};
  }

  const initialScrollTop = target.scrollTop;
  const initialScrollLeft = target.scrollLeft;
  switch (scrollParams.direction) {
    case ScrollDirection.LEFT:
      target.scrollLeft -= scrollParams.distance;
      break;
    case ScrollDirection.RIGHT:
      target.scrollLeft += scrollParams.distance;
      break;
    case ScrollDirection.UP:
      target.scrollTop -= scrollParams.distance;
      break;
    case ScrollDirection.DOWN:
      target.scrollTop += scrollParams.distance;
      break;
    default:
      return {success: false, message: 'Invalid scroll direction.'};
  }

  if (target.scrollTop === initialScrollTop &&
      target.scrollLeft === initialScrollLeft) {
    return {
      success: false,
      message: 'Element scroll position did not change after calling scrollBy.',
    };
  }

  return {success: true, message: 'Successfully scrolled element.'};
}

/**
 * Simulates scrolling at a coordinate.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param pixelType The type of pixels.
 * @param [direction] The scroll direction.
 * @param [distance] The distance to scroll.
 * @return an object containing the result of the scroll attempt.
 */
function scrollByCoordinate(
    x: number,
    y: number,
    pixelType: number,
    direction?: number,
    distance?: number,
    ): {
  success: boolean,
  message: string,
} {
  let {element} = getElementFromPoint(x, y, pixelType);
  if (!element) {
    return {
      success: false,
      message: 'No element found at the target coordinates.',
    };
  }

  let scrollParams: ScrollParams|undefined;
  if (direction !== undefined && distance !== undefined) {
    scrollParams = {
      direction: getScrollDirection(direction),
      distance,
    };

    // When the desktop ScrollTool does a directional scroll of an element
    // targeted by coordinate, it checks if it has a scrollable ancestor and
    // uses that for scrolling.
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/scroll_tool.cc;l=34;drc=06d06050f1a98a6ce06cc1dc4470eebaf5a81990
    const scrollableElement = findScrollableAncestor(element, direction);
    if (!scrollableElement) {
      return {
        success: false,
        message: 'Element has no scrollable ancestor.',
      };
    }
    element = scrollableElement;
  }

  return scrollElement(element, scrollParams);
}

/**
 * Simulates scrolling an element specified by its DOM node ID.
 * @param nodeId The ID of the node.
 * @param [direction] The scroll direction.
 * @param [distance] The distance to scroll.
 * @return an object containing the result of the scroll attempt.
 */
function scrollByNodeId(
    nodeId: number, direction?: number, distance?: number): {
  success: boolean,
  message: string,
} {
  let node: Node|null = null;
  if (nodeId === 0) {
    node = document.scrollingElement;
  } else {
    node = getNodeById(nodeId, window);
  }

  if (!node) {
    return {
      success: false,
      message: `No element found with id ${nodeId}.`,
    };
  }

  if (node.nodeType !== Node.ELEMENT_NODE) {
    // The desktop ScrollTool treats targets that are not ELEMENT nodes as
    // invalid. See
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/scroll_tool.cc;l=34;drc=06d06050f1a98a6ce06cc1dc4470eebaf5a81990.
    return {
      success: false,
      message: `Node with id ${nodeId} is not an element.`,
    };
  }

  let scrollParams: ScrollParams|undefined;
  if (direction !== undefined && distance !== undefined) {
    scrollParams = {
      direction: getScrollDirection(direction),
      distance,
    };
  }

  return scrollElement(node as Element, scrollParams);
}

const scrollToolApi = new CrWebApi('scroll_tool');
scrollToolApi.addFunction('scrollByCoordinate', scrollByCoordinate);
scrollToolApi.addFunction('scrollByNodeId', scrollByNodeId);
scrollToolApi.addFunction('isScrollable', isScrollable);

gCrWeb.registerApi(scrollToolApi);
