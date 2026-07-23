// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onconnect = e => {
  const port = e.ports[0];
  port.onmessage = async e => {
    port.postMessage(self.navigator.globalPrivacyControl === true);
  };
};
