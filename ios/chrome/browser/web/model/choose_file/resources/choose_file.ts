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

// Processes a click and forwards it to `processHTMLInputElementClick` if the
// target of `inputEvent` is indeed an HTMLInputElement.
function processWindowClickEvent(inputEvent: MouseEvent): void {
  if (!windowClickEventListenerEnabled) {
    return;
  }
  if (!inputEvent.target || !(inputEvent.target instanceof HTMLInputElement)) {
    return;
  }
  const target = inputEvent.target as HTMLInputElement;
  const message = processHTMLInputElementClick(target);
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
    // The original implementation will trigger the window click event listener
    // if `document.contains(this)` is true, hence the guards to disable the
    // listener while already processing the click below.
    windowClickEventListenerEnabled = false;
    originalHTMLInputElementClick.call(this);
    windowClickEventListenerEnabled = true;
    const message = processHTMLInputElementClick(this);
    if (message) {
      sendWebKitMessage(CHOOSE_FILE_INPUT_HANDLER_NAME, message);
    }
  };
}

registerHTMLInputElementClickListener();
