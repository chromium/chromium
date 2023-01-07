// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.getElementById('connectForm').addEventListener('submit', doConnect);
  document.getElementById('sendForm').addEventListener('submit', doSend);
  document.getElementById('listenForm').addEventListener('submit', doListen);
  document.getElementById('closeButton').addEventListener('click', doClose);
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

var msgTcpCreate = 't;'
var msgUdpCreate = 'u;'
var msgSend = 's;'
var msgClose = 'c;'
var msgListen = 'l;'

function doConnect(event) {
  // Send a request message. See also socket.cc for the request format.
  event.preventDefault();
  var hostname = document.getElementById('hostname').value;
  var type = document.getElementById('connect_type').value;
  common.logMessage(type);
  if (type == 'tcp') {
    common.naclModule.postMessage(msgTcpCreate + hostname);
  } else {
    common.naclModule.postMessage(msgUdpCreate + hostname);
  }
}

function doSend(event) {
  // Send a request message. See also socket.cc for the request format.
  event.preventDefault();
  var message = document.getElementById('message').value;
  while (message.indexOf('\\n') > -1)
    message = message.replace('\\n', '\n');
  common.naclModule.postMessage(msgSend + message);
}

function doListen(event) {
  // Listen a the given port.
  event.preventDefault();
  var port = document.getElementById('port').value;
  var type = document.getElementById('listen_type').value;
  common.naclModule.postMessage(msgListen + port);
}

function doClose() {
  // Send a request message. See also socket.cc for the request format.
  common.naclModule.postMessage(msgClose);
}

function handleMessage(message) {
  common.logMessage(message.data);
}
