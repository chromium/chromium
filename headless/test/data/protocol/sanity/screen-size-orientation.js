// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests screen size orientation.');

  const result = await session.evaluate('window.screen.orientation.type');
  testRunner.log('orientation=' + result);

  testRunner.completeTest();
})
