// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for the WebActuationTool to perform actions on elements.
 */

import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actuation/model/tools/resources/actuation_utils.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

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
    clickCount: number): {success: boolean, message: string} {
  // clickType: 1=LEFT, 2=RIGHT maps to 0, 2 in MouseEvent.button.
  let button = 0;
  if (clickType === 1) {
    button = 0;
  } else if (clickType === 2) {
    button = 2;
  }

  const touch = new Touch({
    identifier: 0,
    target: element,
    clientX: clientX,
    clientY: clientY,
  });

  const touchEventInit: TouchEventInit = {
    bubbles: true,
    cancelable: false,
    view: window,
    touches: [touch],
    targetTouches: [touch],
    changedTouches: [touch],
  };

  const dispatchEvents = (detail: number): boolean => {
    const mouseEventInit: MouseEventInit = {
      bubbles: true,
      cancelable: false,
      view: window,
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
      success: false,
      message: 'Failed to dispatch click event sequence.',
    };
  }

  // For double click, dispatch the same series of events followed by a dblclick
  // event.
  if (clickCount === 2) {
    // This event sequence has detail=2 since we've already clicked once.
    if (!dispatchEvents(/*detail=*/ 2)) {
      return {
        success: false,
        message: 'Failed to dispatch click event sequence again.',
      };
    }
    const dblClickInit: MouseEventInit = {
      bubbles: true,
      cancelable: false,
      view: window,
      detail: 2,
      clientX: clientX,
      clientY: clientY,
      button: button,
    };
    if (!element.dispatchEvent(new MouseEvent('dblclick', dblClickInit))) {
      return {success: false, message: 'Failed to dispatch dblclick event.'};
    }
  }

  return {success: true, message: 'Dispatched touch, mouse, and click events.'};
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
    pixelType: number): {success: boolean, message: string} {
  const {element, clientX, clientY} = getElementFromPoint(x, y, pixelType);

  if (!element) {
    return {
      success: false,
      message: 'No element found at the target coordinates.',
    };
  }
  if (element.tagName.toUpperCase() === 'IFRAME') {
    return {
      success: false,
      message: 'iframe found at the target coordinates.',
    };
  }

  return dispatchClickEvents(element, clientX, clientY, clickType, clickCount);
}

const clickToolApi = new CrWebApi('click_tool');
clickToolApi.addFunction('clickByCoordinate', clickByCoordinate);
gCrWeb.registerApi(clickToolApi);
