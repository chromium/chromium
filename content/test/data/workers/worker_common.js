// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Track the number of clients for this worker - tests can use this to ensure
// that shared workers are actually shared, not distinct.
var num_clients = 0;

if (!self.postMessage) {
  // This is a shared worker - mimic dedicated worker APIs
  onconnect = function(event) {
    num_clients++;
    event.ports[0].onmessage = function(e) {
      self.postMessage = function(msg) {
        event.ports[0].postMessage(msg);
      };
      self.onmessage(e);
    };
  };
} else {
  num_clients++;
}
onmessage = function(evt) {
  if (evt.data == "ping") {
    postMessage("pong");
  } else if (evt.data == "auth") {
    importScripts("/auth-basic");
  } else if (evt.data == "close") {
    close();
  } else if (/eval.+/.test(evt.data)) {
    try {
      postMessage(eval(evt.data.substr(5)));
    } catch (ex) {
      postMessage(ex);
    }
  } else if (/tls-client-auth-import.+/.test(evt.data)) {
    try {
      importScripts(evt.data.substr(23));
    } catch (ex) {
    }
    postMessage("done");
  } else if (/tls-client-auth-fetch.+/.test(evt.data)) {
    fetch(evt.data.substr(22)).then(_ => {}, _ => postMessage("done"));
  }
}
