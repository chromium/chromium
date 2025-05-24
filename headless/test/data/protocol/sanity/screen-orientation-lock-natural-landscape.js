// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `<!doctype html>
  <html>
    <script>
      screen.orientation.onchange = () => {
        console.log('Locked screen orientation: '
                 + screen.orientation.type);
      };

      async function lockOrientation(orientation) {
        console.log('Locking screen orientation ' + orientation);
        return screen.orientation.lock(orientation);
      }
    </script>
    <body></body>
  </html>
  `;

  const {session, dp} = await testRunner.startHTML(
      html, 'Tests screen orientation lock with natural landscape device.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  const result = await session.evaluate('window.screen.orientation.type');
  testRunner.log('Initial screen orientation ' + result);

  async function tryOrientationLock(orientation) {
    return session.evaluateAsync(`window.lockOrientation('${orientation}')`);
  }

  await tryOrientationLock('portrait-primary');
  await tryOrientationLock('portrait-secondary');
  await tryOrientationLock('landscape-primary');
  await tryOrientationLock('landscape-secondary');
  await tryOrientationLock('natural');

  testRunner.completeTest();
})
