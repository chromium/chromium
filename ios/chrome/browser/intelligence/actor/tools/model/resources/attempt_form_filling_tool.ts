// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript API for AttemptFormFillingTool.
 */

import {RENDERER_ID_NOT_SET} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {isAutofillableElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import {getUniqueID} from '//components/autofill/ios/form_util/resources/fill_util.js';
import type {ActionTarget, Coordinate} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getElementFromPoint, isCoordinateTarget, isNodeIdTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getNodeById} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// LINT.IfChange(AttemptFormFillingToolResultCode)
export enum AttemptFormFillingToolResultCode {
  // The lookup was successful and the renderer ID was resolved.
  OK = 0,
  // The provided target is invalid.
  INVALID_TARGET = 1,
  // The provided coordinates are out of bounds or did not match any element on
  // the page.
  COORDINATES_OUT_OF_BOUNDS = 2,
  // The provided DOM node ID could not be found or did not resolve to an
  // element.
  INVALID_DOM_NODE_ID = 3,
  // The provided target is not an autofillable element.
  TARGET_NOT_AUTOFILL_ELEMENT = 4,
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h:AttemptFormFillingToolResultCode)

interface AutofillRendererIdResult {
  resultCode: AttemptFormFillingToolResultCode;
  uniqueId?: string;
}

/**
 * Resolves the Autofill renderer ID for the element.
 *
 * @param element The target element.
 * @return An object containing the result code and the unique ID if successful.
 */
function getAutofillRendererIdByElement(element: Element):
    AutofillRendererIdResult {
  if (!isAutofillableElement(element)) {
    return {
      resultCode: AttemptFormFillingToolResultCode.TARGET_NOT_AUTOFILL_ELEMENT,
    };
  }
  const uniqueId = getUniqueID(element);
  if (!uniqueId || uniqueId === RENDERER_ID_NOT_SET) {
    return {
      resultCode: AttemptFormFillingToolResultCode.TARGET_NOT_AUTOFILL_ELEMENT,
    };
  }
  return {
    resultCode: AttemptFormFillingToolResultCode.OK,
    uniqueId,
  };
}

/**
 * Resolves the Autofill renderer ID for the element matching `nodeId`.
 *
 * @param nodeId The DOM node ID of the target element.
 * @return An object containing the result code and the unique ID if successful.
 */
function getAutofillRendererIdByNodeId(nodeId: number):
    AutofillRendererIdResult {
  const node = getNodeById(nodeId, window);
  if (!node || node.nodeType !== Node.ELEMENT_NODE) {
    return {
      resultCode: AttemptFormFillingToolResultCode.INVALID_DOM_NODE_ID,
    };
  }
  return getAutofillRendererIdByElement(node as Element);
}

/**
 * Resolves the Autofill renderer ID for the element at the specified
 * coordinates.
 *
 * @param coordinate The target coordinate.
 * @return An object containing the result code and the unique ID if successful.
 */
function getAutofillRendererIdByCoordinate(coordinate: Coordinate):
    AutofillRendererIdResult {
  const fromPoint = getElementFromPoint(coordinate);
  if (!fromPoint.element) {
    return {
      resultCode: AttemptFormFillingToolResultCode.COORDINATES_OUT_OF_BOUNDS,
    };
  }
  return getAutofillRendererIdByElement(fromPoint.element as Element);
}

interface AutofillRendererIdsResult {
  resultCode: AttemptFormFillingToolResultCode;
  uniqueIds: string[];
}

/**
 * Batched resolver that retrieves Autofill renderer IDs for a list of targets.
 *
 * @param targets A list of target specifications (ActionTarget).
 * @return A list of resolution results matching the input targets list.
 */
function getAutofillRendererIds(targets: ActionTarget[]):
    AutofillRendererIdsResult {
  const uniqueIds: string[] = [];
  for (const target of targets) {
    let result: AutofillRendererIdResult;

    // Attempt to retrieve the autofill renderer ID for `target`.
    if (isNodeIdTarget(target)) {
      result = getAutofillRendererIdByNodeId(target.contentNodeId);
    } else if (isCoordinateTarget(target)) {
      result = getAutofillRendererIdByCoordinate(target.coordinate);
    } else {
      // Invalid target.
      result = {
        resultCode: AttemptFormFillingToolResultCode.INVALID_TARGET,
      };
    }

    // Immediately return the result code if the autofill renderer ID for the
    // current target could not be retrieved.
    if (result.resultCode !== AttemptFormFillingToolResultCode.OK) {
      return {
        resultCode: result.resultCode,
        uniqueIds: [],
      };
    }
    uniqueIds.push(result.uniqueId!);
  }
  return {
    resultCode: AttemptFormFillingToolResultCode.OK,
    uniqueIds,
  };
}

const api = new CrWebApi('attempt_form_filling');
api.addFunction('getAutofillRendererIds', getAutofillRendererIds);
gCrWeb.registerApi(api);
