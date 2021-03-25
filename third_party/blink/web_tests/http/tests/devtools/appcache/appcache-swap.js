// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that application cache model keeps track of manifest urls and statuses correctly after UPDATEREADY event and swapCache() call.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('resources');

  var frameId1;
  var frameId2;

  await UI.viewManager.showView('resources');
  ApplicationTestRunner.startApplicationCacheStatusesRecording();
  ApplicationTestRunner.dumpApplicationCache();
  ApplicationTestRunner.createAndNavigateIFrame('resources/with-versioned-manifest.php', step1);

  function step1(frameId) {
    frameId1 = frameId;
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId1, 'resources/versioned-manifest.php', applicationCache.IDLE, step2);
  }

  function step2() {
    ApplicationTestRunner.dumpApplicationCache();
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/versioned-manifest.php?command=step', true, step3);
  }

  function step3() {
    ApplicationTestRunner.createAndNavigateIFrame('resources/with-versioned-manifest.php', step4);
  }

  function step4(frameId) {
    frameId2 = frameId;
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId1, 'resources/versioned-manifest.php', applicationCache.UPDATEREADY, step5);
  }

  function step5(frameId) {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId2, 'resources/versioned-manifest.php', applicationCache.UPDATEREADY, step6);
  }

  function step6() {
    ApplicationTestRunner.dumpApplicationCache();

    ApplicationTestRunner.swapFrameCache(frameId1);
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId1, 'resources/versioned-manifest.php', applicationCache.IDLE, step7);
  }

  function step7() {
    ApplicationTestRunner.dumpApplicationCache();
    TestRunner.completeTest();
  }
})();
