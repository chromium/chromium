// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that application cache model keeps track of manifest urls and statuses correctly when there is a non existing file listed in manifest. https://bugs.webkit.org/show_bug.cgi?id=64581\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var frameId1;
  var frameId2;
  var frameId3;

  await UI.viewManager.showView('resources');
  ApplicationTestRunner.startApplicationCacheStatusesRecording();
  ApplicationTestRunner.dumpApplicationCache();
  ApplicationTestRunner.createAndNavigateIFrame(
      'resources/page-with-manifest.php?manifestId=with-non-existing-file', step1);

  function step1(frameId) {
    frameId1 = frameId;
    // Waiting for at least two status events notifications from backend, to make sure
    // we are not completing test before loading application cache.
    ApplicationTestRunner.ensureFrameStatusEventsReceived(frameId, 2, step2);
  }

  function step2() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(frameId1, '', applicationCache.UNCACHED, step3);
  }

  function step3() {
    ApplicationTestRunner.dumpApplicationCache();
    TestRunner.completeTest();
  }
})();
