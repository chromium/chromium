// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

// Called by the common.js module.
function attachListeners() {
  document.getElementById('button').addEventListener('click', loadUrl);
}

function loadUrl() {
  common.naclModule.postMessage('getUrl:url_loader_success.html');
}

// Called by the common.js module.
function handleMessage(message_event) {
  var logEl = document.getElementById('output');
  // Find the first line break.  This separates the URL data from the
  // result text.  Note that the result text can contain any number of
  // '\n' characters, so split() won't work here.
  var url = message_event.data;
  var result = '';
  var eolPos = message_event.data.indexOf('\n');
  if (eolPos != -1) {
    url = message_event.data.substring(0, eolPos);
    if (eolPos < message_event.data.length - 1) {
      result = message_event.data.substring(eolPos + 1);
    }
  }
  logEl.textContent += 'FULLY QUALIFIED URL: ' + url + '\n';
  logEl.textContent += 'RESULT:\n' + result + '\n';
}
