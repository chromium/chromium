// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.sendMessage({type: 'getData'}, (response) => {
  // A successful response should contain the expected data and no error.
  const success =
      !chrome.runtime.lastError && response && response.data === 'success';

  if (!success) {
    console.error(
        'Messaging stress test: iframe failed to receive response.',
        chrome.runtime.lastError);
  }

  parent.postMessage(success ? 'success' : 'fail', '*');
});
