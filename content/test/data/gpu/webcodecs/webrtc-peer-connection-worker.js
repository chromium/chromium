// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('webcodecs_common.js');

self.onmessage = async e => {
  const source = new CanvasSource(e.data.width, e.data.height);
  const writer = e.data.writable.getWriter();
  // write frames continuously until test is completed
  setInterval(async () => {
    await writer.write(await source.getNextFrame());
  }, 100);
};
