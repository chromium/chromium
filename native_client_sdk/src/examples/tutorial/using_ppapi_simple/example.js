// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Once we load, hide the plugin
function moduleDidLoad() {
  common.hideModule();
}

// Called by the common.js module.
// nacl_io/ppapi_simple generates two different types of messages:
// - messages from /dev/tty (prefixed with PS_TTY_PREFIX)
// - exit message (prefixed with PS_EXIT_MESSAGE)
function handleMessage(message) {
  if (message.data.indexOf("exit:") == 0) {
    // When we receive the exit message we post an empty reply back to
    // confirm, at which point the module will exit.
    message.srcElement.postMessage({"exit" : ""});
  } else if (message.data.indexOf("tty:") == 0) {
    common.logMessage(message.data.slice("tty:".length));
  } else {
    console.log("Unhandled message: " + message.data);
  }
}
