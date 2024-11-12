// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Annoying hack: the test suite modifies the event page idle timeout to be
// 1 millisecond to avoid waiting multiple seconds for the event page to be
// torn down. However, tabs.create() will not wait for the tab to be *loaded*
// before returning. The test expects that the tab will keep the event page
// alive, which it will, but only after it commits to the extension origin.
// To work around this, keep the event page alive by consistently querying until
// it's finished loading (having an outstanding extension API function like
// tabs.get() will keep the page alive in the meantime).
async function waitForTabLoaded(tab) {
  state = tab.status;
  while (state !== 'complete') {
    await new Promise((resolve) => {
      chrome.tabs.get(tab.id, (updated) => {
        state = updated.status;
        resolve();
      });
    });
  }
}

chrome.runtime.onInstalled.addListener(function() {
  chrome.tabs.create({url: 'popup.html'}, (tab) => {
    waitForTabLoaded(tab);
  });
});
