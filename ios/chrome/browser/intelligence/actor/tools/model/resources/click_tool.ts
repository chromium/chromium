// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for the ActorTool to perform actions on elements.
 */

import type {ActionTarget, Coordinate} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getElementFromPoint, isCoordinateTarget, isNodeIdTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
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

  // Events are configured as cancelable so that pages can prevent default
  // browser actions via `preventDefault()`.
  const touchEventInit: TouchEventInit = {
    bubbles: true,
    cancelable: true,
    view: elementWindow,
    touches: [touch],
    targetTouches: [touch],
    changedTouches: [touch],
  };

  const accepted: string[] = [];
  const suppressed: string[] = [];

  /**
   * Dispatches a touch event sequence, and dispatches emulated mouse events if
   * touches were not canceled.
   * @param detail The click count detail for mouse events.
   * @return True if touch events were not canceled and mouse events were
   *     dispatched.
   */
  const dispatchEvents = (detail: number): boolean => {
    accepted.push('touch');
    const touchstartAllowed =
        element.dispatchEvent(new TouchEvent('touchstart', touchEventInit));
    const touchendAllowed =
        element.dispatchEvent(new TouchEvent('touchend', touchEventInit));

    // If either 'touchstart' or 'touchend' are canceled, don't dispatch mouse
    // events per https://w3c.github.io/touch-events/#mouse-events.
    const mouseEventsAllowed = touchstartAllowed && touchendAllowed;
    if (mouseEventsAllowed) {
      accepted.push('mouse', 'click');
      const mouseEventInit: MouseEventInit = {
        bubbles: true,
        cancelable: true,
        view: elementWindow,
        detail: detail,
        clientX: clientX,
        clientY: clientY,
        button: button,
      };
      element.dispatchEvent(new MouseEvent('mousemove', mouseEventInit));
      element.dispatchEvent(new MouseEvent('mousedown', mouseEventInit));
      element.dispatchEvent(new MouseEvent('mouseup', mouseEventInit));
      element.dispatchEvent(new MouseEvent('click', mouseEventInit));
    } else {
      suppressed.push('mouse', 'click');
    }
    return mouseEventsAllowed;
  };

  const firstMouseEventsAllowed = dispatchEvents(/*detail=*/ 1);

  // For double click, dispatch a second touch event sequence followed by a
  // dblclick event if both touch sequences permitted emulated mouse events.
  if (clickCount === 2) {
    // If the first click's touch sequence was canceled, no mouse events were
    // dispatched, so the consecutive click count detail resets to 1 per
    // https://developer.mozilla.org/en-US/docs/Web/API/UIEvent/detail.
    const secondDetail = firstMouseEventsAllowed ? 2 : 1;
    const secondMouseEventsAllowed = dispatchEvents(/*detail=*/ secondDetail);
    // A 'dblclick' mouse event is only dispatched if both touch sequences
    // permitted emulated mouse events.
    if (firstMouseEventsAllowed && secondMouseEventsAllowed) {
      accepted.push('dblclick');
      const dblClickInit: MouseEventInit = {
        bubbles: true,
        cancelable: true,
        view: elementWindow,
        detail: 2,
        clientX: clientX,
        clientY: clientY,
        button: button,
      };
      element.dispatchEvent(new MouseEvent('dblclick', dblClickInit));
    } else {
      suppressed.push('dblclick');
    }
  }

  const acceptedSummary = Array.from(new Set(accepted)).join(', ');
  let message = `Dispatched ${acceptedSummary} events.`;
  if (suppressed.length > 0) {
    const suppressedSummary = Array.from(new Set(suppressed)).join(', ');
    message += ` Suppressed: ${suppressedSummary}.`;
  }

  return {
    resultCode: ClickToolResultCode.OK,
    message: message,
  };
}

/**
 * Simulates a click on an element at the specified coordinates.
 * @param coordinate The target coordinate.
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @return An object containing the result of the click attempt.
 */
function clickByCoordinate(
    coordinate: Coordinate, clickType: number,
    clickCount: number): {resultCode: number, message: string} {
  const target = getElementFromPoint(coordinate);
  if (!target.element) {
    return {
      resultCode: ClickToolResultCode.COORDINATES_OUT_OF_BOUNDS,
      message: 'Point is outside of the viewport.',
    };
  }

  return dispatchClickEvents(
      target.element, target.clientX, target.clientY, clickType, clickCount);
}

/**
 * Simulates a click on an element with the given DOM node ID.
 * @param nodeId The DOM node ID of the target element.
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @return an object containing the result of the click attempt.
 */
function clickByNodeId(nodeId: number, clickType: number, clickCount: number):
    {resultCode: number, message: string} {
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

/**
 * Simulates a click on an element specified by an ActionTarget.
 * @param target The action target (coordinate or node ID).
 * @param clickType The type of click (0=UNKNOWN, 1=LEFT, 2=RIGHT).
 * @param clickCount The number of clicks (0=UNKNOWN, 1=SINGLE, 2=DOUBLE).
 * @return an object containing the result of the click attempt.
 */
function click(target: ActionTarget, clickType: number, clickCount: number):
    {resultCode: number, message: string} {
  if (isNodeIdTarget(target)) {
    return clickByNodeId(target.contentNodeId, clickType, clickCount);
  } else if (isCoordinateTarget(target)) {
    return clickByCoordinate(target.coordinate, clickType, clickCount);
  }

  return {
    resultCode: ClickToolResultCode.COORDINATES_OUT_OF_BOUNDS,
    message: 'Invalid target.',
  };
}

const clickToolApi = new CrWebApi('click_tool');
clickToolApi.addFunction('click', click);
gCrWeb.registerApi(clickToolApi);
