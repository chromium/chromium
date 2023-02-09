// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `<!doctype html>
    <html><body>
    <input type="text" id="input" value="input_value" autofocus>
    </body></html>
  `;

  const {page, session, dp} = await testRunner.startHTML(
      html, `Tests input field clipboard operations.`);

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

  const modControl = 2;
  const modCommand = 4;
  const mod = navigator.platform.includes('Mac') ? modCommand : modControl;

  await logElementValue("input");
  await sendKey('a', 65, mod, ['selectAll']);
  await sendKey('c', 67, mod, ['copy']);

  await sendKey('1', 61);
  await sendKey('2', 62);
  await sendKey('3', 63);
  await logElementValue("input");

  // Don't send Ctrl+A here because this would cause clipboard copy on
  // systems that support selection clipboard, e.g. Linux.
  await sendKey('v', 86, mod, ['paste']);
  await logElementValue("input");

  testRunner.completeTest();
})
