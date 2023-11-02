// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.getElementById('fileInput').addEventListener('change',
      handleFileInput);
  document.getElementById('listener').addEventListener('drop',
      handleFileDrop, true);
}

function postFileContents(file) {
  var reader = new FileReader();
  reader.onload = function(load_event) {
    if (common.naclModule)
      common.naclModule.postMessage(load_event.target.result);
  }
  reader.readAsArrayBuffer(file);
}

// Handle a file being dropped on to the plugin's rectangle.
function handleFileDrop(dropEvent) {
  if (!dropEvent.dataTransfer || !dropEvent.dataTransfer.files)
    return;
  dropEvent.stopPropagation();
  dropEvent.preventDefault();
  var files = dropEvent.dataTransfer.files;
  for(var i = 0; i < files.length; ++i)
    postFileContents(files[i]);
}

// Handle a file being chosen from the <input type=file...> tag.
function handleFileInput() {
  var file_input = document.getElementById("fileInput");
  var files = file_input.files;
  for(var i = 0; i < files.length; ++i)
    postFileContents(files[i]);
}
