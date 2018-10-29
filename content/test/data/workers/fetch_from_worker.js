// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = async e => {
  try {
    const response = await fetch(e.data.url);
    if (!response.ok) {
      self.postMessage('bad response');
      return;
    }
    const text = await response.text();
    self.postMessage(text);
  } catch (error) {
    self.postMessage(`${error}`);
  }
};
