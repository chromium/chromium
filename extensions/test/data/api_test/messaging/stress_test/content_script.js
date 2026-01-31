// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // This test creates a large number of iframes, each of which will send a
  // message to the background script and expect an asynchronous response as a
  // stress test. The test passes only if all responses are successfully
  // received.
  async function stressTest() {
    // The number of iframes to inject. Each iframe sends one message.
    // 25 is used here to avoid timeouts in the test infrastructure, while
    // still providing enough concurrency to exercise the fix.
    const kNumIframes = 25;
    let successes = 0;
    let failures = 0;

    // Listen for completion messages from the injected iframes.
    window.addEventListener('message', (event) => {
      if (event.data === 'success') {
        successes++;
      } else if (event.data === 'fail') {
        failures++;
      }

      // Once all iframes have reported back, check if any failed.
      if (successes + failures === kNumIframes) {
        if (failures === 0) {
          chrome.test.succeed();
        } else {
          chrome.test.fail(
              `Failed: ${failures} out of ${kNumIframes} messages failed.`);
        }
      }
    });

    // Inject iframes.
    for (let i = 0; i < kNumIframes; i++) {
      const iframe = document.createElement('iframe');
      iframe.src = chrome.runtime.getURL('iframe.html');
      document.body.appendChild(iframe);
    }
  }
]);
