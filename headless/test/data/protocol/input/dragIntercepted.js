// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <a href="https://example.com">Drag Me!</a>
  `, `Tests Input.dragIntercepted event.`);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  dumpError(await dp.Input.setInterceptDrags({
    enabled: true,
  }));

  const dragInterceptedPromise = new Promise(fulfill => {
    dp.Input.onDragIntercepted(fulfill);
  });

  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: 20,
    y: 20
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseMoved',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: 150,
    y: 150
  }));
  const result = await dragInterceptedPromise;
  testRunner.log(JSON.stringify(result.params.data, undefined, 2));

  testRunner.completeTest();
});
