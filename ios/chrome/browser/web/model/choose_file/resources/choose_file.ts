// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Observes taps on Choose file inputs.
 */

import {processHTMLInputElementClick} from '//ios/chrome/browser/web/model/choose_file/resources/choose_file_utils.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

const CHOOSE_FILE_INPUT_HANDLER_NAME = 'ChooseFileHandler';

// Whether the window click event listener should forward events through WebKit
// messages.
let windowClickEventListenerEnabled = true;

// The last click pointer event in the page with the associated timestamp.
let lastPointerEvent: PointerEvent|null = null;
let lastPointerEventTimeStamp = 0;

// Processes a click and forwards it to `processHTMLInputElementClick` if the
// target of `inputEvent` is indeed an HTMLInputElement.
function processWindowClickEvent(pointerEvent: PointerEvent): void {
  if (!windowClickEventListenerEnabled) {
    return;
  }
  lastPointerEvent = pointerEvent;
  lastPointerEventTimeStamp = performance.now();
  if (!pointerEvent.target ||
      !(pointerEvent.target instanceof HTMLInputElement)) {
    return;
  }
  const target = pointerEvent.target as HTMLInputElement;
  const message = processHTMLInputElementClick(target, lastPointerEvent);
  if (message) {
    sendWebKitMessage(CHOOSE_FILE_INPUT_HANDLER_NAME, message);
  }
}

// Registers passive event listeners for the click on choose file inputs.
function registerWindowClickEventListener(): void {
  window.addEventListener(
      'click', processWindowClickEvent, {capture: true, passive: true});
}

registerWindowClickEventListener();

// Registers passive event listeners for the click on choose file inputs.
function registerHTMLInputElementClickListener(): void {
  const originalHTMLInputElementClick = HTMLInputElement.prototype.click;
  HTMLInputElement.prototype.click = function(this: HTMLInputElement) {
    try {
      let pointerEvent: PointerEvent|null = null;
      if (performance.now() < lastPointerEventTimeStamp + 1) {
        // The last pointer event is only used if it is less than 1 second old.
        pointerEvent = lastPointerEvent;
      }
      const message = processHTMLInputElementClick(this, pointerEvent);
      if (message) {
        sendWebKitMessage(CHOOSE_FILE_INPUT_HANDLER_NAME, message);
      }
    } catch (e) {
    }
    // The original implementation will trigger the window click event listener
    // if `document.contains(this)` is true, hence the guards to disable the
    // listener while already processing the click below.
    windowClickEventListenerEnabled = false;
    originalHTMLInputElementClick.call(this);
    windowClickEventListenerEnabled = true;
  };
}

registerHTMLInputElementClickListener();
