// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<style>
        #frame {
          width: 200px;
          height: 200px;
        }
      </style>`,
      `Tests that LayerTree.enable does not crash the renderer with an out-of-process iframe.`);

  await session.evaluateAsync(
      url => new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.id = 'frame';
        iframe.src = url;
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      }),
      'http://localhost:8000/inspector-protocol/layers/resources/composited-iframe.html');

  await dp.LayerTree.enable();
  await dp.LayerTree.onceLayerTreeDidChange();
  // Make sure the renderer is still alive.
  testRunner.log(`Evaluated: ${await session.evaluate('1 + 1')}`);
  testRunner.completeTest();
});
