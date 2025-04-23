// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Observes taps on Choose file inputs.
 */

import {processHTMLInputElementClick} from '//ios/chrome/browser/web/model/choose_file/resources/choose_file_utils.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

const CHOOSE_FILE_INPUT_HANDLER_NAME = 'ChooseFileHandler';

// Processes a click and forwards it to `processHTMLInputElementClick` if the
// target of `inputEvent` is indeed an HTMLInputElement.
function processWindowClickEvent(inputEvent: MouseEvent): void {
  if (!inputEvent.target || !(inputEvent.target instanceof HTMLInputElement)) {
    return;
  }
  const message = processHTMLInputElementClick(inputEvent.target);
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
