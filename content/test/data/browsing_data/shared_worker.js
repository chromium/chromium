// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let clients = new Array();
let value = null;

// This shared worker allows to store and retrieve values across tabs.
self.onconnect = e => {
  let port = e.ports[0];
  clients.push(port);
  port.onmessage = e => {
    if (e.data.value) {
      value = e.data.value;
    } else {
      for (let client of clients)
        client.postMessage({ "value": value });
    }
  };
  port.start();
  port.postMessage({ "connected": true });
};
