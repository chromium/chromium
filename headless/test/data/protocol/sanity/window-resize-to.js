// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests window outer ' +
      'size is properly adjusted by `window.resizeTo()`.');
  const initialSize = await session.evaluate('({outerWidth, outerHeight})');
  testRunner.log(initialSize, 'Outer window size (initial): ');

  const resizePromise = session.evaluateAsync(`
    new Promise(resolve =>
        {window.addEventListener('resize', resolve, {once: true})})
  `);
  await session.evaluate('window.resizeTo(700, 500)');
  await resizePromise;

  const finalSize = await session.evaluate('({outerWidth, outerHeight})');
  testRunner.log(finalSize, 'Outer window size (final): ');
  testRunner.completeTest();
});
