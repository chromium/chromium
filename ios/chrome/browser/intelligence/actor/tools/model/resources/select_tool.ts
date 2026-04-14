// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions to allow Chrome on iOS to select an option in a
 * <select> element.
 */

import {setInputElementValue, valueForElement} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Simulates selecting the <option> with the given value in a <select> element.
 *
 * This function will set the value of the <select> element and emit events that
 * mimic a manual user selection if the following criteria are met:
 * - The <select> element is not disabled
 * - The <select> element has an option with `targetValue` as its value
 * - The <option> with the targeted value is not disabled
 *
 * These criteria were mirrored from the Desktop SelectTool implementation, see
 * https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/actor/select_tool.cc;l=35;drc=e8b169d1e8ed51cc6e49a169f10c4876e5a9e30f.
 */
function selectMatchingOption(element: HTMLElement, targetValue: string): {
  success: boolean,
  message: string,
} {
  if (element.tagName.toUpperCase() !== 'SELECT') {
    return {
      success: false,
      message: 'Target element is not a <select>.',
    };
  }

  const selectElement = element as HTMLSelectElement;
  if (selectElement.disabled) {
    return {
      success: false,
      message: '<select> element is disabled.',
    };
  }

  let originalTargetOptionValue: string|null = null;
  for (let i = 0; i < selectElement.options.length; i++) {
    const option = selectElement.options[i] as HTMLOptionElement;
    // Trim the whitespace from the option value to match the behavior of
    // Blink's HTMLOptionElement::value which is used by the Desktop SelectTool:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/forms/html_option_element.cc;l=302-308;drc=8abea14deda089834ba142a35e8342014812df55
    if (valueForElement(option) === targetValue) {
      if (option.disabled) {
        return {
          success: false,
          message: 'Specified value to select does exist but is disabled.',
        };
      }
      originalTargetOptionValue = option.value;
      break;
    }
  }

  if (!originalTargetOptionValue) {
    return {
      success: false,
      message: `Option with value '${targetValue}' not found.`,
    };
  }

  // We use setInputElementValue to set the element value following Autofill's
  // example, dispatching the necessary events and ensuring the element is made
  // active.
  //
  // `setInputElementValue` takes in a HTMLInputElement but supports <select>
  // elements under the hood, so we use this improper cast here.
  //
  // See the fillFormField method in autofill_controller.js, which calls
  // `setInputElementValue` with an argument that is actually a <select>:
  // https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/resources/autofill_controller.js;l=597-598;drc=c96942249c46055e50b3288518331c13912249fa.
  setInputElementValue(originalTargetOptionValue, element as HTMLInputElement);

  return {success: true, message: 'Selected option successfully.'};
}

/**
 * Tries to find the <select> element by coordinate and select the option with
 * the given `value`.
 */
function selectByCoordinate(
    x: number, y: number, pixelType: number, value: string): {
  success: boolean,
  message: string,
} {
  const {element} = getElementFromPoint(x, y, pixelType);
  if (!element || !(element instanceof HTMLElement)) {
    return {
      success: false,
      message: 'No element found at the target coordinates.',
    };
  }
  return selectMatchingOption(element as HTMLElement, value);
}

/**
 * Tries to find the <select> element by its DOM node id and select the option
 * with the given `value`.
 */
function selectByNodeId(nodeId: number, value: string): {
  success: boolean,
  message: string,
} {
  const node: Node|null = getNodeById(nodeId, window);
  if (!node || node.nodeType !== Node.ELEMENT_NODE) {
    return {
      success: false,
      message: `No element found with id ${nodeId}.`,
    };
  }
  if (!(node instanceof HTMLElement)) {
    return {
      success: false,
      message: `Element with id ${nodeId} is not an HTMLElement.`,
    };
  }
  return selectMatchingOption(node as HTMLElement, value);
}

const selectToolApi = new CrWebApi('select_tool');
selectToolApi.addFunction('selectByCoordinate', selectByCoordinate);
selectToolApi.addFunction('selectByNodeId', selectByNodeId);
gCrWeb.registerApi(selectToolApi);
