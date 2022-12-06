// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function readWholeBuffer(buffer) {
  let bytes = new Uint8Array(buffer);
  return bytes.reduce((acc, byte) => {
    return byte ? acc + 1 : acc;
  }, 0);
}

self.onmessage = function(e) {
  const buf = e.data;
  readWholeBuffer(buf);
  if (buf instanceof SharedArrayBuffer)
    postMessage(buf, []);
  else
    postMessage(buf, [buf]);
}
