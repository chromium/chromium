// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
* @fileoverview Adds listeners that forward keypress and paste events to the
* browser. The browser uses this information to detect and warn the user about
* situations where the user enters one of their saved passwords on a
* possibly-unsafe site
*/

/**
 * Listens for keypress events and forwards the entered key to the browser.
 */
function onKeypressEvent(event) {
  // Only forward events where the entered key has length 1, to avoid forwarding
  // special keys like "Enter".
  if (event.isTrusted && event.key.length == 1) {
    window.webkit.messageHandlers['PasswordProtectionTextEntered'].postMessage(
        {eventType: 'KeyPressed', text: event.key});
  }
}

/**
 * Listens for paste events and forwards the pasted text to the browser.
 */
function onPasteEvent(event) {
  if (event.isTrusted) {
    var text = event.clipboardData.getData('text');
    window.webkit.messageHandlers['PasswordProtectionTextEntered'].postMessage(
        {eventType: 'TextPasted', text: text});
  }
}

// Events are first dispatched to the window object, in the capture phase of
// JavaScript event dispatch, so listen for them there.
window.addEventListener('keypress', onKeypressEvent, true);
window.addEventListener('paste', onPasteEvent, true);
