// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `<!doctype html>
    <html> <body>
      <input type="text" id="input1" value="input1_value">
      <input type="text" id="input2" value="input2_value">
    </body></html>
  `;

  const {page, session, dp} = await testRunner.startHTML(
      html, 'Tests clipboard copy/paste operations.');

  async function logElementValue(id) {
    const value = await session.evaluate(`
      document.getElementById("${id}").value;
    `);
    testRunner.log(`${id}: ${value}`);
  }

  await dp.Browser.grantPermissions({permissions: ['clipboardReadWrite']});

  await logElementValue('input1');
  await logElementValue('input2');

  await session.evaluateAsync(() => {
    const input = document.getElementById('input1');
    navigator.clipboard.writeText(input.value);
  });

  await session.evaluateAsync(async () => {
    const text = await navigator.clipboard.readText();
    document.getElementById('input2').value = text;
  });

  testRunner.log(`Copied input1 value to input2`);

  await logElementValue('input1');
  await logElementValue('input2');

  testRunner.completeTest();
})
