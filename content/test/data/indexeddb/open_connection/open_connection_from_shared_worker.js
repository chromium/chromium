// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

onconnect = function(e) {
  var port = e.ports[0];

  port.onmessage = async function(e) {
    let connected;
    let connected_promise = new Promise(r => { connected = r; });
    let request = indexedDB.open("Foo", 1);

    request.onsuccess = e => {
      connected(e.target.result);
    };
    request.onerror = e => {
      connected(undefined);
    }

    let connection = await connected_promise;
    if (connection)
      connection.close();

    port.postMessage({rqid: e.data.rqid, result: connection != undefined});
  }

}
