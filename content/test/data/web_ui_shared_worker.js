// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onconnect = (event) => {
  const port = event.ports[0];
  port.onmessage = (e) => {
    if (e.data === 'ping') {
      port.postMessage('pong');
    }
  };
};
