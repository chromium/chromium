// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onconnect = function (event) {
  const port = event.ports[0];
  port.onmessage = function(e) {
    port.postMessage('reply ' + e.data);
  };
};