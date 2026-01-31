// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  if (request.type === 'getData') {
    // Simulate a long-running asynchronous task (e.g., network request, heavy
    // computation) before responding. The delay is set to several seconds to
    // ensure many responses remain "pending" in the browser process to elicit
    // the previous callback-reuse issue. See crrev.com/c/6819178 for more
    // details.
    const delayMs = 3000;
    setTimeout(() => {
      sendResponse({data: 'success'});
    }, delayMs);

    // Return true to indicate that we will respond asynchronously.
    return true;
  }
});
