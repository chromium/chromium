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
let lastBlurredElement_: HTMLElement|null = null;

/**
 * The list of observed elements.
 */
let observedElements_: Element[] = [];

/*
 * Returns whether an element is of a type we wish to observe.
 * Must be in sync with what is supported in showBottomSheet_.
 * @private
 */
function isObservable_(element: HTMLElement): boolean {
  // Ignore passkey fields, which contain the 'webauthn' autofill tag.
  const autocomplete_attribute = element.getAttribute('autocomplete');
  const isPasskeyField = autocomplete_attribute?.includes('webauthn');
  return ((element instanceof HTMLInputElement) ||
          (element instanceof HTMLFormElement)) &&
      !isPasskeyField;
}

/*
 * Prepare and send message to show bottom sheet.
 * @private
 */
function showBottomSheet_(hasUserGesture: boolean): void {
  // Verify that the window's layout viewport has a height and a width and also
  // that the element is visible.
  if (window.innerHeight == 0 || window.innerWidth == 0 ||
      !gCrWeb.fill.isVisibleNode(lastBlurredElement_)) {
    return;
  }

  let field = null;
  let fieldType = '';
  let fieldValue = '';
  let form = null;

  if (lastBlurredElement_ instanceof HTMLInputElement) {
    field = lastBlurredElement_;
    fieldType = lastBlurredElement_.type;
    fieldValue = lastBlurredElement_.value;
    form = lastBlurredElement_.form;
  } else if (lastBlurredElement_ instanceof HTMLFormElement) {
    form = lastBlurredElement_;
  }

  // TODO(crbug.com/40261693): convert these "gCrWeb.fill" and "gCrWeb.form"
  // calls to import and call the functions directly once the conversion to
  // TypeScript is done.

  const msg = {
    'frameID': gCrWeb.message.getFrameId(),
    'formName': gCrWeb.form.getFormIdentifier(form),
    'formRendererID': gCrWeb.fill.getUniqueID(form),
    'fieldIdentifier': gCrWeb.form.getFieldIdentifier(field),
    'fieldRendererID': gCrWeb.fill.getUniqueID(field),
    'fieldType': fieldType,
    'type': 'focus',
    'value': fieldValue,
    'hasUserGesture': hasUserGesture,
  };
  sendWebKitMessage('BottomSheetMessage', msg);
}

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

  // Field must be empty (ignoring white spaces).
  if ((event.target instanceof HTMLInputElement) && event.target.value.trim()) {
    return;
  }

  // Prevent the keyboard from showing up.
  event.target.blur();
  lastBlurredElement_ = event.target;

  showBottomSheet_(event.isTrusted);
}

/**
 * Removes listeners on the elements associated with each provided renderer ID
 * and removes those same elements from list of observed elements.
 * @private
 */
function detachListeners_(renderer_ids: number[]): void {
  for (const renderer_id of renderer_ids) {
    const element = gCrWeb.fill.getElementByUniqueID(renderer_id);
    let index = observedElements_.indexOf(element);
    if (index > -1) {
      element.removeEventListener('focus', focusEventHandler_, true);
      observedElements_.splice(index, 1);
    }
  }
}

/**
 * Finds the element associated with each provided renderer ID and
 * attaches a listener to each of these elements for the focus event.
 * "allow_autofocus" specifies whether the bottom sheet can be triggered by an
 * already focused field.
 */
function attachListeners(
    renderer_ids: number[], allow_autofocus: boolean): void {
  // Build list of elements
  let blurredElement: HTMLElement|null = null;
  let elementsToObserve: Element[] = [];
  for (const renderer_id of renderer_ids) {
    const element = gCrWeb.fill.getElementByUniqueID(renderer_id);
    // Only add element to list of observed elements if we aren't already
    // observing it.
    if (element && isObservable_(element) &&
        !observedElements_.find(elem => elem === element)) {
      elementsToObserve.push(element);
      if (document.activeElement === element) {
        // Check if the field is empty (ignoring white spaces).
        if (element.value.trim() != '') {
          // The user has already started filling the active field, so bail out
          // without attaching listeners.
          return;
        }
        if (allow_autofocus) {
          // Remove the focus on an element if it already has focus and we want
          // to show the related bottom sheet immediately.
          element.blur();
          blurredElement = element;
        }
      }
    }
  }

  // Attach the listeners once the IDs are set.
  for (const element of elementsToObserve) {
    element.addEventListener('focus', focusEventHandler_, true);
    observedElements_.push(element);
  }

  // Restore focus if it was removed.
  if (blurredElement) {
    lastBlurredElement_ = blurredElement;
    showBottomSheet_(/*hasUserGesture=*/ false);
  }
}

/**
 * Removes all previously attached listeners before re-triggering
 * a focus event on the previously blurred element.
 */
function detachListeners(renderer_ids: number[], refocus: boolean): void {
  // If the bottom sheet was dismissed, we don't need to show it anymore on this
  // page, so remove the event listeners.
  detachListeners_(renderer_ids);

  if (refocus && lastBlurredElement_) {
    // Re-focus the previously blurred element.
    lastBlurredElement_.focus();
  }
}

gCrWeb.bottomSheet = {
  attachListeners,
  detachListeners
};
