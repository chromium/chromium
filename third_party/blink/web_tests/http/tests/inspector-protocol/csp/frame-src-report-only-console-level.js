// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that the Log.entryAdded for a report-only frame-src violation has the info level.`);

  const entries = [];
  dp.Log.onEntryAdded(({params}) => {
    if (params.entry.source === 'security') {
      entries.push(params.entry);
    }
  });
  await dp.Log.enable();

  await session.navigate('resources/frame-src-report-only.php');

  entries.forEach((entry, index) => {
    testRunner.log(
        `Message[${index}]: ${entry.source} ${entry.level}: ${entry.text}`);
  });
  testRunner.completeTest();
});
