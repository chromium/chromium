// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests screen scale factor.');

  const result = await session.evaluate('window.devicePixelRatio');
  testRunner.log('devicePixelRatio=' + result);

  testRunner.completeTest();
})
