// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for determining the target frame for an action.
 */

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {getElementFromPoint} from '//ios/chrome/browser/intelligence/actuation/model/tools/resources/actuation_utils.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Resolves the target iframe at the given coordinates.
 *
 * If the element at the coordinates is an iframe, it registers the child frame
 * and returns the details needed to forward the action (remote token and
 * frame-relative coordinates) via the `childFrame` property. If the element
 * is not an iframe, the `childFrame` property is simply omitted, indicating
 * that the target resides within the current frame.
 *
 * @param {number} x The x-coordinate.
 * @param {number} y The y-coordinate.
 * @param {number} pixelType The type of pixels (0=UNSPECIFIED, 1=DIPS,
 *     2=PHYSICAL).
 * @return An object containing the result of the resolution attempt.
 */
function resolveTargetIframe(x: number, y: number, pixelType: number): {
  success: boolean,
  message?: string,
  childFrame?: {
    remoteFrameToken: string,
    frameX: number,
    frameY: number,
  },
} {
  const {element, clientX, clientY} = getElementFromPoint(x, y, pixelType);

  if (!element) {
    return {
      success: false,
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
      success: true,
      childFrame: {
        remoteFrameToken: token,
        frameX: clientX - rect.left - borderLeft - paddingLeft,
        frameY: clientY - rect.top - borderTop - paddingTop,
      },
    };
  }
  return {
    success: true,
  };
}

const api = new CrWebApi('actuation_target');
api.addFunction('resolveTargetIframe', resolveTargetIframe);
gCrWeb.registerApi(api);
