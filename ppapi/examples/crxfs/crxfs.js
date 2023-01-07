// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function StartRequest() {
  var plugin = document.getElementById("plugin");
  var filename = document.getElementById("filename").value;
  plugin.postMessage(filename);
}

function HandleMessage(message_event) {
  document.getElementById("content").value = message_event.data;
}

document.addEventListener('DOMContentLoaded', function () {
  // Attach a listener for the message event. This must happen after the plugin
  // object was created.
  document.getElementById("plugin")
      .addEventListener("message", HandleMessage, false);

  document.getElementById("start")
      .addEventListener("click", StartRequest, false);
});
