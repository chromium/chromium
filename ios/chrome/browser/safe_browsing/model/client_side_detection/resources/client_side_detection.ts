// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Adds copy listener that forwards clipboard copy events to the
 * browser for Safe Browsing client-side phishing detection.
 */

/**
 * Returns the currently selected text. Checks `document.activeElement` for
 * `<input>` and `<textarea>` controls because `window.getSelection()` does not
 * capture selections inside text form fields per the DOM specification.
 */
function getSelectedText(): string {
  const activeElement = document.activeElement;
  if (activeElement instanceof HTMLInputElement ||
      activeElement instanceof HTMLTextAreaElement) {
    const start = activeElement.selectionStart ?? 0;
    const end = activeElement.selectionEnd ?? 0;
    return activeElement.value.substring(start, end);
  }
  const selection = window.getSelection();
  return selection ? selection.toString() : '';
}

/**
 * Handles copy events by extracting selected text and forwarding it to the
 * browser. Only trusted (user-initiated) events with non-empty text within the
 * maximum allowed length are forwarded; synthetic or untrusted events are
 * ignored.
 */
function onCopyEvent(event: Event): void {
  if (!event.isTrusted) {
    return;
  }

  // Maximum length of copied text to extract and forward to the browser.
  // Protects IPC transport from oversized payloads while allowing the browser
  // to record relevant metrics.
  const kMaxCopiedTextLength = 65535;

  const copiedText = getSelectedText();
  if (copiedText.length === 0 || copiedText.length > kMaxCopiedTextLength) {
    return;
  }
  sendWebKitMessage('ClientSideDetectionMessage', {copyText: copiedText});
}

// Events are first dispatched to the window object, in the capture phase of
// JavaScript event dispatch, so listen for them there.
window.addEventListener('copy', onCopyEvent, /*useCapture=*/ true);
