// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for performing type actions on elements.
 */

import {setInputElementValue, valueForElement} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actuation/model/tools/resources/actuation_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';


/**
 * Updates the text in element and sends events to simulate manual typing.
 * @param element The target element.
 * @param text The text to type.
 * @param mode The type mode (1=DELETE_EXISTING, 2=PREPEND, 3=APPEND) from
 *             optimization_guide::Action::TypeAction::TypeMode in
 *             components/optimization_guide/proto/features/actions_data.proto.
 * @param followByEnter Whether to press enter after typing.
 * @return an object containing the result of the type attempt.
 */
function updateElementAndDispatchTypeEvents(
    element: HTMLElement, text: string, mode: number,
    followByEnter: boolean): {success: boolean, message: string} {
  if (!(element instanceof HTMLInputElement ||
        element instanceof HTMLTextAreaElement || element.isContentEditable)) {
    return {
      success: false,
      message: 'Target element is not an input, textarea, or contentEditable.',
    };
  }

  const isInput = element instanceof HTMLInputElement;
  const isTextArea = element instanceof HTMLTextAreaElement;

  let currentText = '';
  if (isInput) {
    currentText = valueForElement(element as HTMLInputElement);
  } else if (isTextArea) {
    currentText = (element as HTMLTextAreaElement).value;
  } else {
    currentText = element.innerText;
  }

  let newText = '';
  switch (mode) {
    case 1:  // DELETE_EXISTING
      newText = text;
      break;
    case 2:  // PREPEND
      newText = text + currentText;
      break;
    case 3:  // APPEND
      newText = currentText + text;
      break;
    default: {
      return {
        success: false,
        message: 'Invalid type mode. Must be in [1,3].',
      };
    }
  }

  let success = false;
  if (isInput) {
    success = setInputElementValue(newText, element as HTMLInputElement);
  } else if (isTextArea) {
    (element as HTMLTextAreaElement).value = newText;
    success = (element as HTMLTextAreaElement).value === newText;
  } else {
    // Fallback to editing innertext if targeting an element that isn't an
    // input or textarea. This is not ideal but is the best way of supporting
    // elements that have contenteditable.
    element.innerText = newText;
    success = element.innerText === newText;
  }

  if (!isInput) {
    // Dispatch events manually when not sent by the Autofill
    // `setInputElementValue`.
    const events = ['keydown', 'keypress', 'input', 'keyup', 'change'];
    events.forEach(type => {
      element.dispatchEvent(
          new Event(type, {bubbles: true, cancelable: false}));
    });
  }

  if (!success) {
    return {
      success,
      message: 'Failed to type the text into the element.',
    };
  }

  if (followByEnter) {
    const enterEventInit: KeyboardEventInit = {
      bubbles: true,
      cancelable: false,
      key: 'Enter',
      code: 'Enter',
      keyCode: 13,
      which: 13,
    };
    element.dispatchEvent(new KeyboardEvent('keydown', enterEventInit));
    element.dispatchEvent(new KeyboardEvent('keypress', enterEventInit));
    element.dispatchEvent(new Event('input', {bubbles: true}));
    element.dispatchEvent(new KeyboardEvent('keyup', enterEventInit));
    element.dispatchEvent(new Event('change', {bubbles: true}));
  }

  return {success: true, message: 'Dispatched type events.'};
}

/**
 * Simulates typing at a coordinate.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param pixelType The type of pixels.
 * @param text The text to type.
 * @param mode The type mode.
 * @param followByEnter Whether to press enter.
 * @return an object containing the result of the type attempt.
 */
function typeByCoordinate(
    x: number, y: number, pixelType: number, text: string, mode: number,
    followByEnter: boolean): {
  success: boolean,
  message: string,
} {
  const {element} = getElementFromPoint(x, y, pixelType);

  if (!element || !(element instanceof HTMLElement)) {
    return {
      success: false,
      message: 'No focusable element found at the target coordinates.',
    };
  }

  return updateElementAndDispatchTypeEvents(element, text, mode, followByEnter);
}

/**
 * Simulates typing into an element specified by its DOM node ID.
 * @param nodeId The ID of the node.
 * @param text The text to type.
 * @param mode The type mode.
 * @param followByEnter Whether to press enter.
 * @return an object containing the result of the type attempt.
 */
function typeByNodeId(
    nodeId: number, text: string, mode: number, followByEnter: boolean): {
  success: boolean,
  message: string,
} {
  const node = getNodeById(nodeId, window);
  if (!node) {
    return {
      success: false,
      message: `No element found with id ${nodeId}.`,
    };
  }

  let element: HTMLElement;
  if (node instanceof HTMLElement) {
    element = node;
  } else if (node.parentElement instanceof HTMLElement) {
    element = node.parentElement;
  } else {
    return {
      success: false,
      message: `Node with id ${nodeId} is not a focusable HTMLElement.`,
    };
  }

  return updateElementAndDispatchTypeEvents(element, text, mode, followByEnter);
}

const typeToolApi = new CrWebApi('type_tool');
typeToolApi.addFunction('typeByCoordinate', typeByCoordinate);
typeToolApi.addFunction('typeByNodeId', typeByNodeId);
gCrWeb.registerApi(typeToolApi);
