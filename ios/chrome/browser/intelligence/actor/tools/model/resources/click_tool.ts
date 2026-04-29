// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for the ActorTool to perform actions on elements.
 */

import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// LINT.IfChange(ClickToolResultCode)
export enum ClickToolResultCode {
  // The function call was successful.
  OK = 0,
  // The coordinates provided to the function were not in the viewport.
  COORDINATES_OUT_OF_BOUNDS = 1,
  // The DOM node ID is invalid or did not resolve to a clickable element.
  INVALID_DOM_NODE_ID = 2,
  // The targeted element is disabled.
  ELEMENT_DISABLED = 3,
  // The click event was not able to be dispatched.
  CLICK_SUPPRESSED = 4,
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h:ClickToolResultCode)

/**
 * Dispatches touch and mouse events to simulate a click on an element.
 * @param element The target element.
 * @param clientX The client x-coordinate.
 * @param clientY The client y-coordinate.
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @return an object containing the result of the click attempt.
 */
function dispatchClickEvents(
    element: Element, clientX: number, clientY: number, clickType: number,
    clickCount: number): {resultCode: number, message: string} {
  // Check if the element is disabled, following Desktop's example. See:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/click_tool.cc;l=128-137;drc=b8b4703f515083fc684d0ee0d959ac51467877bb
  if ((element as any).disabled) {
    return {
      resultCode: ClickToolResultCode.ELEMENT_DISABLED,
      message: 'Target element is disabled.',
    };
  }

  // clickType: 1=LEFT, 2=RIGHT maps to 0, 2 in MouseEvent.button.
  let button = 0;
  if (clickType === 1) {
    button = 0;
  } else if (clickType === 2) {
    button = 2;
  }

  const elementWindow = element.ownerDocument?.defaultView ?? window;
  const touch = new Touch({
    identifier: 0,
    target: element,
    clientX: clientX,
    clientY: clientY,
  });

  const touchEventInit: TouchEventInit = {
    bubbles: true,
    cancelable: false,
    view: elementWindow,
    touches: [touch],
    targetTouches: [touch],
    changedTouches: [touch],
  };

  const dispatchEvents = (detail: number): boolean => {
    const mouseEventInit: MouseEventInit = {
      bubbles: true,
      cancelable: false,
      view: elementWindow,
      detail: detail,
      clientX: clientX,
      clientY: clientY,
      button: button,
    };
    return element.dispatchEvent(
               new TouchEvent('touchstart', touchEventInit)) &&
        element.dispatchEvent(new TouchEvent('touchend', touchEventInit)) &&
        element.dispatchEvent(new MouseEvent('mousemove', mouseEventInit)) &&
        element.dispatchEvent(new MouseEvent('mousedown', mouseEventInit)) &&
        element.dispatchEvent(new MouseEvent('mouseup', mouseEventInit)) &&
        element.dispatchEvent(new MouseEvent('click', mouseEventInit));
  };

  if (!dispatchEvents(/*detail=*/ 1)) {
    return {
      resultCode: ClickToolResultCode.CLICK_SUPPRESSED,
      message: 'Failed to dispatch click event sequence.',
    };
  }

  // For double click, dispatch the same series of events followed by a dblclick
  // event.
  if (clickCount === 2) {
    // This event sequence has detail=2 since we've already clicked once.
    if (!dispatchEvents(/*detail=*/ 2)) {
      return {
        resultCode: ClickToolResultCode.CLICK_SUPPRESSED,
        message: 'Failed to dispatch click event sequence again.',
      };
    }
    const dblClickInit: MouseEventInit = {
      bubbles: true,
      cancelable: false,
      view: elementWindow,
      detail: 2,
      clientX: clientX,
      clientY: clientY,
      button: button,
    };
    if (!element.dispatchEvent(new MouseEvent('dblclick', dblClickInit))) {
      return {
        resultCode: ClickToolResultCode.CLICK_SUPPRESSED,
        message: 'Failed to dispatch dblclick event.',
      };
    }
  }

  return {
    resultCode: ClickToolResultCode.OK,
    message: 'Dispatched touch, mouse, and click events.',
  };
}

/**
 * Simulates a click on a coordinate.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @param pixelType The type of pixels (0=UNSPECIFIED, 1=DIPS, 2=PHYSICAL).
 * @return an object containing the result of the click attempt.
 */
function clickByCoordinate(
    x: number, y: number, clickType: number, clickCount: number,
    pixelType: number): {
  resultCode: number,
  message: string,
} {
  const {element, clientX, clientY} = getElementFromPoint(x, y, pixelType);

  if (!element) {
    return {
      resultCode: ClickToolResultCode.COORDINATES_OUT_OF_BOUNDS,
      message: 'Point is outside of the viewport.',
    };
  }
  return dispatchClickEvents(element, clientX, clientY, clickType, clickCount);
}

/**
 * Simulates a click on an element specified by its DOM node ID.
 * @param nodeId The ID of the node.
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @return an object containing the result of the click attempt.
 */
function clickByNodeId(nodeId: number, clickType: number, clickCount: number): {
  resultCode: number,
  message: string,
} {
  const node = getNodeById(nodeId, window);
  if (!node) {
    return {
      resultCode: ClickToolResultCode.INVALID_DOM_NODE_ID,
      message: `No element found with id ${nodeId}.`,
    };
  }

  let element: Element;
  if (node.nodeType === Node.ELEMENT_NODE) {
    element = node as Element;
  } else if (node.nodeType === Node.TEXT_NODE && node.parentElement) {
    element = node.parentElement;
  } else {
    return {
      resultCode: ClickToolResultCode.INVALID_DOM_NODE_ID,
      message: `Node with id ${nodeId} is not clickable.`,
    };
  }

  // Click at the center of the element.
  const rect = element.getBoundingClientRect();
  const clientX = rect.x + (rect.width / 2);
  const clientY = rect.y + (rect.height / 2);

  return dispatchClickEvents(element, clientX, clientY, clickType, clickCount);
}

const clickToolApi = new CrWebApi('click_tool');
clickToolApi.addFunction('clickByCoordinate', clickByCoordinate);
clickToolApi.addFunction('clickByNodeId', clickByNodeId);
gCrWeb.registerApi(clickToolApi);
