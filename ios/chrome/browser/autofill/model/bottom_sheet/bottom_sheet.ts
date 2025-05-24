// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';
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
const observedElements_: Element[] = [];

/*
 * Returns whether an element is of a type we wish to observe.
 * Must be in sync with what is supported in showBottomSheet.
 * @private
 */
function isObservable(element: HTMLElement): boolean {
  // Ignore passkey fields, which contain the 'webauthn' autofill tag.
  const autocomplete_attribute = element.getAttribute('autocomplete');
  const isPasskeyField = autocomplete_attribute?.includes('webauthn');
  return ((element instanceof HTMLInputElement) ||
          (element instanceof HTMLFormElement)) &&
      !isPasskeyField;
}

/*
 * Returns true if the bottom sheet can be triggered.
 * @private
 */
function canTriggerBottomSheet(focusedElement: Element) {
  // Verify that the window's layout viewport has a height and a width and also
  // that the element is visible.
  return window.innerHeight > 0 && window.innerWidth > 0 &&
      gCrWebLegacy.fill.isVisibleNode(focusedElement);
}

/*
 * Prepare and send message to show bottom sheet.
 * @private
 */
function showBottomSheet(hasUserGesture: boolean): void {
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

  // TODO(crbug.com/40261693): convert these "gCrWebLegacy.fill" and
  // "gCrWebLegacy.form" calls to import and call the functions directly once
  // the conversion to TypeScript is done.

  const msg = {
    'frameID': gCrWeb.getFrameId(),
    'formName': gCrWebLegacy.form.getFormIdentifier(form),
    'formRendererID': gCrWebLegacy.fill.getUniqueID(form),
    'fieldIdentifier': gCrWebLegacy.form.getFieldIdentifier(field),
    'fieldRendererID': gCrWebLegacy.fill.getUniqueID(field),
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
function focusEventHandler(event: Event): void {
  if (!event.target || !(event.target instanceof HTMLElement) ||
      (event.target !== document.activeElement)) {
    return;
  }

  // Field must be empty (ignoring white spaces).
  if ((event.target instanceof HTMLInputElement) && event.target.value.trim()) {
    return;
  }

  // Show the bottom sheet iff the conditions are right, or bail out otherwise
  // and let the user fallback to using keyboard for filling the field.
  if (canTriggerBottomSheet(event.target!)) {
    // Prevent the keyboard from showing up iff the bottom sheet can be
    // triggered.
    event.target.blur();
    lastBlurredElement_ = event.target;

    showBottomSheet(event.isTrusted);
  }
}

/**
 * Removes listeners on the elements associated with each provided renderer ID
 * and removes those same elements from list of observed elements.
 * @private
 */
function detachListenersInternal(rendererIds: number[]): void {
  for (const rendererId of rendererIds) {
    const element = gCrWebLegacy.fill.getElementByUniqueID(rendererId);
    const index = observedElements_.indexOf(element);
    if (index > -1) {
      element.removeEventListener('focus', focusEventHandler, true);
      observedElements_.splice(index, 1);
    }
  }
}

/**
 * Finds the element associated with each provided renderer ID and
 * attaches a listener to each of these elements for the focus event.
 * "allowAutofocus" specifies whether the bottom sheet can be triggered by an
 * already focused field.
 */
function attachListeners(rendererIds: number[], allowAutofocus: boolean): void {
  // Build list of elements
  let elementToBlur: HTMLElement|null = null;
  const elementsToObserve: Element[] = [];
  for (const renderer_id of rendererIds) {
    const element = gCrWebLegacy.fill.getElementByUniqueID(renderer_id);
    // Only add element to list of observed elements if we aren't already
    // observing it.
    if (element && isObservable(element) &&
        !observedElements_.find(elem => elem === element)) {
      const autofocused = document.activeElement === element;
      if (allowAutofocus || !autofocused) {
        // Observe element if eligible.
        elementsToObserve.push(element);
      }
      if (autofocused) {
        // Check if the field is empty (ignoring white spaces).
        if (element.value.trim() !== '') {
          // The user has already started filling the active field, so bail out
          // without attaching listeners.
          return;
        }
        if (allowAutofocus) {
          elementToBlur = element;
        }
      }
    }
  }

  // Attach the listeners once the IDs are set.
  for (const element of elementsToObserve) {
    element.addEventListener('focus', focusEventHandler, true);
    observedElements_.push(element);
  }

  // If (1) the element was already focused (i.e. likely autofocused) at the
  // moment of attaching the listeners , (2) taking over autofocus is allowed,
  // and (3) the sheet can be triggered, trigger the bottom sheet immediately
  // and allow restoring the focus later on once the sheet is dismissed.
  if (elementToBlur && canTriggerBottomSheet(elementToBlur!)) {
    // Remove the focus so the sheet can take over filling without conflicting
    // with the keyboard.
    elementToBlur.blur();
    lastBlurredElement_ = elementToBlur;
    showBottomSheet(/*hasUserGesture=*/ false);
  }
}

/**
 * Refocuses on the last element that was blurred by the listeners.
 */
function refocusLastBlurredElement() {
  lastBlurredElement_?.focus();
  lastBlurredElement_ = null;
}

/**
 * Removes all previously attached listeners before re-triggering
 * a focus event on the previously blurred element.
 */
function detachListeners(rendererIds: number[], refocus: boolean): void {
  // If the bottom sheet was dismissed, we don't need to show it anymore on this
  // page, so remove the event listeners.
  detachListenersInternal(rendererIds);

  if (refocus) {
    refocusLastBlurredElement();
  }
}

gCrWebLegacy.bottomSheet = {
  attachListeners,
  detachListeners,
  refocusLastBlurredElement,
};
