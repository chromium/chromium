// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      'Tests Browser.setDownloadBehavior downloadPath validation.');

  const browserProtocol = testRunner.browserP();
  for (const behavior of ['allow', 'allowAndName']) {
    const response =
        await browserProtocol.Browser.setDownloadBehavior({behavior});
    testRunner.log(
        `${behavior} without downloadPath error code: ${response.error?.code}`);
    testRunner.log(`${behavior} without downloadPath error message: ${
        response.error?.message}`);
  }

  for (const behavior of ['allow', 'allowAndName']) {
    testRunner.expectedSuccess(
        `${behavior} with downloadPath is accepted`,
        await browserProtocol.Browser.setDownloadBehavior(
            {behavior, downloadPath: 'downloads'}));
  }
  for (const behavior of ['deny', 'default']) {
    testRunner.expectedSuccess(
        `${behavior} without downloadPath is accepted`,
        await browserProtocol.Browser.setDownloadBehavior({behavior}));
  }
  testRunner.completeTest();
})
