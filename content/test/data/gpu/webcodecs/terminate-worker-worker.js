// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

self.onmessage = async function(e) {
  const frame = e.data;
  const encoder_config = {
    codec: 'vp8',
    hardwareAcceleration: 'prefer-software',
    width: frame.visibleRect.width,
    height: frame.visibleRect.height,
    bitrate: 5000000,
    framerate: 24,
    latencyMode: 'realtime'
  };
  let resolve_callback = null;
  const encoder_init = {
    output(chunk, metadata) {
      resolve_callback(chunk);
    },
    error(e) {}
  };
  const encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);
  encoder.encode(frame);
  await new Promise((resolve) => {
    resolve_callback = resolve;
  });

  postMessage(null, []);

  for (let i = 1; i < 100; i++) {
    const new_frame =
        new VideoFrame(frame, {timestamp: frame.timestamp + 1000 * i});
    encoder.encode(new_frame);
    await new Promise((resolve) => {
      resolve_callback = resolve;
    });
  }
}
