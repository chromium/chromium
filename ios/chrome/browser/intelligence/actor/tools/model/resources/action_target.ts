// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for determining the target frame for an action.
 */

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import type {ActionTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {getElementFromPoint, isCoordinateTarget} from '//ios/chrome/browser/intelligence/actor/tools/model/resources/actor_tool_utils.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// LINT.IfChange(ActionTargetResultCode)
enum ActionTargetResultCode {
  // The function call was successful.
  OK = 0,
  // The coordinates provided to the function were not in the viewport.
  COORDINATES_OUT_OF_BOUNDS = 1,
  // The arguments provided to `resolveTargetIframe` were invalid.
  ARGUMENTS_INVALID = 2,
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h:ActionTargetResultCode)

/**
 * Resolves whether the element specified by the coordinate target is an iframe.
 *
 * If the element at the target coordinates is an iframe, it registers the child
 * frame and returns the details needed to forward the action (remote token and
 * frame-relative coordinates) via the `childFrame` property. If the element
 * is not an iframe, the `childFrame` property is omitted, indicating
 * that the target element resides within the current frame.
 *
 * @param target The coordinate-based action target used to check if the target
 *     element is an iframe.
 * @return An object containing the result of the resolution attempt.
 */
function resolveTargetIframe(target: ActionTarget): {
  resultCode: number,
  message?: string,
  childFrame?: {
    remoteFrameToken: string,
    frameX: number,
    frameY: number,
  },
} {
  if (!isCoordinateTarget(target)) {
    return {
      resultCode: ActionTargetResultCode.ARGUMENTS_INVALID,
      message:
          'Expected `target` to be coordinate based. Node ID targets can be resolved in native code.',
    };
  }

  const {element, clientX, clientY} = getElementFromPoint(target.coordinate);

  if (!element) {
    return {
      resultCode: ActionTargetResultCode.COORDINATES_OUT_OF_BOUNDS,
      message: 'No element found at the target coordinates.',
    };
  }
  if (element.tagName.toUpperCase() === 'IFRAME') {
    const token = registerChildFrame(element as HTMLIFrameElement);
    const rect = element.getBoundingClientRect();
    const borderLeft = element.clientLeft;
    const borderTop = element.clientTop;
    const computedStyle = window.getComputedStyle(element);
    const paddingLeft = parseFloat(computedStyle.paddingLeft) || 0;
    const paddingTop = parseFloat(computedStyle.paddingTop) || 0;
    return {
      resultCode: ActionTargetResultCode.OK,
      childFrame: {
        remoteFrameToken: token,
        frameX: clientX - rect.left - borderLeft - paddingLeft,
        frameY: clientY - rect.top - borderTop - paddingTop,
      },
    };
  }
  return {
    resultCode: ActionTargetResultCode.OK,
  };
}

const api = new CrWebApi('action_target');
api.addFunction('resolveTargetIframe', resolveTargetIframe);
gCrWeb.registerApi(api);
