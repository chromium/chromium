// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/body.html',
      'Tests that showDirectoryPicker does not crash the browser');
  const showResult = await session.evaluateAsync(`
    !!window.showDirectoryPicker()
  `);
  testRunner.log(`Did show dialog: ${showResult}`);
  testRunner.completeTest();
})
