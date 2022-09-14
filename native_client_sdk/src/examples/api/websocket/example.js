// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.getElementById('connectForm').addEventListener('submit', doConnect);
  document.getElementById('sendForm').addEventListener('submit', doSend);
  document.getElementById('closeButton').addEventListener('click', doClose);
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

function doConnect(event) {
  // Send a request message. See also websocket.cc for the request format.
  var url = document.getElementById('url').value;
  common.naclModule.postMessage('o;' + url);
  event.preventDefault();
}

function doSend() {
  // Send a request message. See also websocket.cc for the request format.
  var message = document.getElementById('message').value;
  var type = document.getElementById('is_binary').checked ? 'b;' : 't;';
  common.naclModule.postMessage(type + message);
  event.preventDefault();
}

function doClose() {
  // Send a request message. See also websocket.cc for the request format.
  common.naclModule.postMessage('c;');
}

function handleMessage(message) {
  common.logMessage(message.data);
}
