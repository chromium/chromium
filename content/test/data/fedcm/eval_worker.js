// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('message', async (e) => {
  if (!e.data.nested) {
    e.ports[0].postMessage(await eval(e.data.script));
    return;
  }
  const worker = new Worker('./eval_worker.js');
  worker.postMessage(
    {
      nested: false,
      script: e.data.script,
    },
    [e.ports[0]]
  );
});
