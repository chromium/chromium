// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests screen orientation lock.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
    if (text === 'PASS' || text === 'FAIL') {
      testRunner.completeTest();
    }
  });

  await session.evaluateAsync(async () => {
    console.log('Current screen orientation: ' + screen.orientation.type);
    const lockedScreenOrientation = 'portrait';
    console.log(`Locking screen orientation to '${lockedScreenOrientation}'`);
    screen.orientation.lock(lockedScreenOrientation)
        .then(() => {
          console.log('Locked screen orientation: ' + screen.orientation.type);
          console.log('PASS');
        })
        .catch((error) => {
          console.error(error.message);
          console.log('FAIL');
        });
  });
})
