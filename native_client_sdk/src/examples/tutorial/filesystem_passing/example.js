// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function attachListeners() {
  document.getElementById('choosedir').addEventListener('click', function(e) {
    chrome.fileSystem.chooseEntry({type: 'openDirectory'}, function(entry) {
      if (!entry) {
        // The user cancelled the dialog.
        return;
      }

      // Send the filesystem and the directory path to the NaCl module.
      common.naclModule.postMessage({
        filesystem: entry.filesystem,
        fullPath: entry.fullPath
      });
    });
  }, false);
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();

  // Make sure this example is running as an App. If not, display a warning.
  if (!chrome.fileSystem) {
    common.updateStatus('Error: must be run as an App.');
    return;
  }
}

// Called by the common.js module.
function handleMessage(message_event) {
  common.logMessage(message_event.data);
}
