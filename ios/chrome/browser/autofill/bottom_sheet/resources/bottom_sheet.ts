// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Adds listeners on the focus event, specifically for elements
 * provided through a list of renderer IDs, in order for to allow showing a
 * bottom sheet in that context.
 */

/**
 * The last HTML element that was blurred.
 */
let lastBlurredElement_: HTMLElement|null;

/**
 * The list of observed elements.
 */
let observedElements_: Element[] = [];

/**
 * Focus events for observed input elements are messaged to the main
 * application for broadcast to WebStateObservers.
 * @private
 */
function focusEventHandler_(event: Event): void {
  if (!event.target || !(event.target instanceof HTMLElement) ||
      (event.target !== document.activeElement)) {
    return;
  }

  // Prevent the keyboard from showing up
  event.target.blur();
  lastBlurredElement_ = event.target;

  let field = null;
  let fieldType = '';
  let fieldValue = '';
  let form = null;

  if (event.target instanceof HTMLInputElement) {
    field = event.target;
    fieldType = event.target.type;
    fieldValue = event.target.value;
    form = event.target.form;
  } else if (event.target instanceof HTMLFormElement) {
    form = event.target;
  }

  // TODO(crbug.com/1427221): convert these "gCrWeb.fill" and "gCrWeb.form"
  // calls to import and call the functions directly once the conversion to
  // TypeScript is done.
  gCrWeb.fill.setUniqueIDIfNeeded(field);
  gCrWeb.fill.setUniqueIDIfNeeded(form);

  const msg = {
    'frameID': gCrWeb.message.getFrameId(),
    'formName': gCrWeb.form.getFormIdentifier(form),
    'uniqueFormID': gCrWeb.fill.getUniqueID(form),
    'fieldIdentifier': gCrWeb.form.getFieldIdentifier(field),
    'uniqueFieldID': gCrWeb.fill.getUniqueID(field),
    'fieldType': fieldType,
    'type': event.type,
    'value': fieldValue,
    'hasUserGesture': event.isTrusted,
  };
  sendWebKitMessage('BottomSheetMessage', msg);
}

/**
 * Attach event listeners for relevant elements on the focus event.
 * @private
 */
function attachListeners_(): void {
  for (const element of observedElements_) {
    element.addEventListener('focus', focusEventHandler_, true);
  }
}

/**
 * Removes all listeners and clears the list of observed elements
 * @private
 */
function detachListeners_(): void {
  for (const element of observedElements_) {
    element.removeEventListener('focus', focusEventHandler_, true);
  }
  observedElements_ = [];
}

/**
 * Finds the element associated with each provided renderer ID and
 * attaches a listener to each of these elements for the focus event.
 */
function attachListeners(renderer_ids: number[]): void {
  // Build list of elements
  for (const renderer_id of renderer_ids) {
    const element = gCrWeb.fill.getElementByUniqueID(renderer_id);
    if (element) {
      observedElements_.push(element);
    }
  }

  // Attach the listeners once the IDs are set.
  attachListeners_();
}

/**
 * Removes all previously attached listeners before re-triggering
 * a focus event on the previously blurred element.
 */
function detachListenersAndRefocus(): void {
  // If the form was dismissed, we don't need to show it anymore on this page,
  // so remove the event listeners.
  detachListeners_();

  // Re-focus the previously blurred element
  if (lastBlurredElement_) {
    lastBlurredElement_.focus();
  }
}

gCrWeb.bottomSheet = {
  attachListeners,
  detachListenersAndRefocus
};
