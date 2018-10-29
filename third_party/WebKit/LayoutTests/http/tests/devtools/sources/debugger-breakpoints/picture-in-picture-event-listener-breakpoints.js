// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Picture-in-Picture event listener breakpoints.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <video id="video" src="../../../media/resources/test.ogv"></video>
    `);
  await TestRunner.evaluateInPagePromise(`
      function playVideo()
      {
          var video = document.getElementById("video");
          video.addEventListener("play", onVideoPlay, false);
          video.play();
      }

      function onVideoPlay()
      {
          return 0;
      }

      function requestPictureInPicture()
      {
          var video = document.getElementById("video");
          video.addEventListener("enterpictureinpicture", onEnterPictureInPicture, false);
          video.requestPictureInPicture();
      }

      function onEnterPictureInPicture()
      {
          return 0;
      }

      function exitPictureInPicture()
      {
          var video = document.getElementById("video");
          video.addEventListener("leavepictureinpicture", onLeavePictureInPicture, false);
          document.exitPictureInPicture();
      }

      function onLeavePictureInPicture()
      {
          return 0;
      }
  `);

  var testFunctions = [
    function testPlayVideoEventBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:play', true, 'video');
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('playVideo()');

      function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:play', false, 'video');
        SourcesTestRunner.resumeExecution(next);
      }
    },

    function testEnterPictureInPictureEventBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:enterpictureinpicture', true, 'video');
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('requestPictureInPicture()', true /* userGesture */);

      function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:enterpictureinpicture', false, 'video');
        SourcesTestRunner.resumeExecution(next);
      }
    },

    function testLeavePictureInPictureEventBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:leavepictureinpicture', true, 'video');
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('exitPictureInPicture()');

      function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:leavepictureinpicture', false, 'video');
        SourcesTestRunner.resumeExecution(next);
      }
    }
  ];

  SourcesTestRunner.runDebuggerTestSuite(testFunctions);

  function printEventTargetName(auxData) {
    var targetName = auxData && auxData.targetName;
    if (targetName)
      TestRunner.addResult('Event target: ' + targetName);
    else
      TestRunner.addResult('FAIL: No event target name received!');
  }
})();
