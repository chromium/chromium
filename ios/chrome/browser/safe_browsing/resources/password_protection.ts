// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from "//ios/web/public/js_messaging/resources/utils.js";

/*
* @fileoverview Adds listeners that forward keypress and paste events to the
* browser. The browser uses this information to detect and warn the user about
* situations where the user enters one of their saved passwords on a
* possibly-unsafe site
*/

/**
 * Listens for keypress events and forwards the entered key to the browser.
 */
function onKeypressEvent(event : KeyboardEvent) : void {
  // Only forward events where the entered key has length 1, to avoid forwarding
  // special keys like "Enter".
  if (event.isTrusted && event.key.length == 1) {
    sendWebKitMessage(
        'PasswordProtectionTextEntered',
        {eventType: 'KeyPressed', text: event.key});
  }
}

/**
 * Listens for paste events and forwards the pasted text to the browser.
 */
function onPasteEvent(event : Event) : void {
  if (!(event instanceof ClipboardEvent))
    return;

  if (!event.isTrusted)
    return;

  const clipboardData = event.clipboardData;
  if (!clipboardData)
    return;

  const text = clipboardData.getData('text');
  sendWebKitMessage(
      'PasswordProtectionTextEntered',
      {eventType: 'TextPasted', text: text});
}

// Events are first dispatched to the window object, in the capture phase of
// JavaScript event dispatch, so listen for them there.
window.addEventListener('keypress', onKeypressEvent, true);
window.addEventListener('paste', onPasteEvent, true);
