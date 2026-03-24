// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that CrashReportContext entries can be retrieved via protocol.
`);

  testRunner.log('Populating CrashReportContext in page...');
  await session.evaluateAsync(async () => {
    await window.crashReport.initialize(1024);
    window.crashReport.set('test-key', 'test-value');
    window.crashReport.set('another-key', 'another-value');

    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    await iframe.contentWindow.crashReport.initialize(1024);
    iframe.contentWindow.crashReport.set('iframe-key', 'iframe-value');
  });

  testRunner.log('Requesting entries...');
  const response = await dp.CrashReportContext.getEntries();
  const entries = response.result.entries;

  // Sort entries by key to ensure stable output
  entries.sort((a, b) => a.key.localeCompare(b.key));

  for (const entry of entries) {
    const frameDetails = entry.frameId ? ' (has frameId)' : ' (no frameId)';
    testRunner.log(`${entry.key}: ${entry.value}${frameDetails}`);
  }

  testRunner.completeTest();
})
