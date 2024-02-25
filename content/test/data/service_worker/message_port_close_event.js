// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = async (e) => {
  if (e.data == 'init') {
    self.port = e.ports[0];
    self.port.start();
  } else if (e.data == 'wait for close event') {
    self.port.onclose = () => {
      e.source.postMessage('close event is fired');
    }
  }
}
