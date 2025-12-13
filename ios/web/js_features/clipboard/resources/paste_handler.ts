// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Attaches a paste event listener to the document to notify the
 * browser of paste events. This script should be injected on document
 * recreation to ensure the event listener is always attached.
 */

import {sendDidFinishClipboardReadMessage} from '//ios/web/js_features/clipboard/resources/clipboard_util.js';

document.addEventListener('paste', () => {
  // Use setTimeout to send the message in the next runloop. This allows the
  // paste event to complete and the content to be pasted into the DOM before
  // the browser is notified.
  setTimeout(sendDidFinishClipboardReadMessage, 0);
});
