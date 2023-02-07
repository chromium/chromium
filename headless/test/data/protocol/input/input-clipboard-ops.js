// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests input field clipboard operations.`);

  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('/resources/input.html')});
  await dp.Page.onceLoadEventFired();

  async function logElementValue(id) {
    const value = await session.evaluate(`
      document.getElementById("${id}").value;
    `);
    testRunner.log(`${id}: ${value}`);
  }

  async function sendKey(text, nativeVirtualKeyCode,
      modifiers = 0, commands = []) {
    await dp.Input.dispatchKeyEvent({
      type: 'keyDown',
      modifiers: modifiers,
      text: text,
      nativeVirtualKeyCode: nativeVirtualKeyCode,
      commands: commands
    });

    await dp.Input.dispatchKeyEvent({
      type: 'keyUp',
      modifiers: modifiers,
      nativeVirtualKeyCode: nativeVirtualKeyCode
    });
  }

  await logElementValue("input");
  await sendKey('a', 65, 2, ["selectAll"]);
  await sendKey('c', 67, 2, ["copy"]);

  await sendKey('a', 65);
  await sendKey('b', 66);
  await sendKey('c', 67);
  await logElementValue("input");

  await sendKey('a', 65, 2, ["selectAll"]);
  await sendKey('c', 67, 2, ["paste"]);
  await logElementValue("input");

  testRunner.completeTest();
})

