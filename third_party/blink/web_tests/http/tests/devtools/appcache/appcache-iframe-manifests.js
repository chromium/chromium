// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that application cache model keeps track of manifest urls and statuses correctly. https://bugs.webkit.org/show_bug.cgi?id=64581\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var frameId1;
  var frameId2;
  var frameId3;

  await UI.viewManager.showView('resources');
  ApplicationTestRunner.dumpApplicationCache();
  ApplicationTestRunner.createAndNavigateIFrame('resources/page-with-manifest.php?manifestId=1', step1);

  function step1(frameId) {
    frameId1 = frameId;
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId1, 'manifest.php?manifestId=1', applicationCache.IDLE, step2);
  }

  function step2() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.navigateIFrame(frameId1, 'resources/page-with-manifest.php?manifestId=2', step3);
  }

  function step3() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId1, 'manifest.php?manifestId=2', applicationCache.IDLE, step4);
  }

  function step4() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.createAndNavigateIFrame('resources/page-with-manifest.php?manifestId=1', step5);
  }

  function step5(frameId) {
    frameId2 = frameId;
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId2, 'manifest.php?manifestId=1', applicationCache.IDLE, step6);
  }

  function step6() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.createAndNavigateIFrame('resources/page-with-manifest.php?manifestId=1', step7);
  }

  function step7(frameId) {
    frameId3 = frameId;
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId3, 'manifest.php?manifestId=1', applicationCache.IDLE, step8);
  }

  function step8() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(
        frameId2, 'manifest.php?manifestId=1', applicationCache.IDLE, step9);
  }

  function step9() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.removeIFrame(frameId3, step10);
  }

  function step10() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(frameId3, '', applicationCache.UNCACHED, step11);
  }

  function step11() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.removeIFrame(frameId2, step12);
  }

  function step12() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(frameId2, '', applicationCache.UNCACHED, step13);
  }

  function step13() {
    ApplicationTestRunner.dumpApplicationCache();
    ApplicationTestRunner.removeIFrame(frameId1, step14);
  }

  function step14() {
    ApplicationTestRunner.waitForFrameManifestURLAndStatus(frameId1, '', applicationCache.UNCACHED, step15);
  }

  function step15() {
    ApplicationTestRunner.dumpApplicationCache();
    TestRunner.completeTest();
  }
})();
