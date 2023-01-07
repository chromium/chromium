// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function takeScreenshot(onScreenshot) {
  console.log('Taking screenshot.');
  chrome.tabs.captureVisibleTab(null, {format: 'png'}, function(img) {
    console.log('Got screenshot, returning...');
    onScreenshot(img);
  });
}

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete')
    return;

  chrome.tabs.executeScript(tabId,
                            {file: 'injected.js', runAt: 'document_start'});
});

chrome.runtime.onMessage.addListener(
    function(request, sender, sendResponse) {
      takeScreenshot(sendResponse);

      // Keep the sendResponse channel open, so a response can be sent
      // asynchronously.
      return true;
    });
