// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests event listener breakpoints.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" id="test">
      <video id="video" src="../../../media/resources/test.ogv"></video>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testElementEventListener()
      {
          return 0;
      }

      function addListenerAndClick()
      {
          var element = document.getElementById("test");
          element.addEventListener("click", testElementEventListener, true);
          element.click();
      }

      function addListenerAndAuxclick()
      {
          var element = document.getElementById("test");
          element.addEventListener("auxclick", testElementEventListener, true);
          element.dispatchEvent(new MouseEvent("auxclick", {button: 1}));
      }

      function addListenerAndPointerDown()
      {
          var element = document.getElementById("test");
          element.addEventListener("pointerdown", testElementEventListener, true);
          element.dispatchEvent(new PointerEvent("pointerdown"));
      }

      function timerFired()
      {
          return 0;
      }

      function addLoadListeners()
      {
          var xhr = new XMLHttpRequest();
          xhr.onload = loadCallback;
          xhr.onerror = loadCallback;
          xhr.open("GET", "/", true);

          var img = new Image();
          img.onload = sendXHR;
          img.onerror = sendXHR;
          img.src = "foo/bar/dummy";

          function sendXHR()
          {
              xhr.send();
          }
      }

      function loadCallback()
      {
          return 0;
      }

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
  `);

  var testFunctions = [
    function testClickBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:click', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('addListenerAndClick()');

      async function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:click', false);
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        TestRunner.evaluateInPage('addListenerAndClick()', next);
      }
    },

    function testAuxclickBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:auxclick', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('addListenerAndAuxclick()');

      async function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:auxclick', false);
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        TestRunner.evaluateInPage('addListenerAndAuxclick()', next);
      }
    },

    function testTimerFiredBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout.callback', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPage('setTimeout(timerFired, 10)');

      async function paused(callFrames) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout.callback', false);
        SourcesTestRunner.resumeExecution(next);
      }
    },

    function testLoadBreakpointOnXHR(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:load', true, 'xmlhttprequest');
      SourcesTestRunner.setEventListenerBreakpoint('listener:error', true, 'xmlhttprequest');
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('addLoadListeners()');

      async function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:load', false, 'xmlhttprequest');
        SourcesTestRunner.setEventListenerBreakpoint('listener:error', false, 'xmlhttprequest');
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        TestRunner.evaluateInPage('addLoadListeners()', next);
      }
    },

    function testMediaEventBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:play', true, 'audio');
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('playVideo()');

      async function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:play', false, 'audio');
        SourcesTestRunner.resumeExecution(next);
      }
    }
  ];

  if (window.PointerEvent) {
    testFunctions.push(function testPointerEventBreakpoint(next) {
      SourcesTestRunner.setEventListenerBreakpoint('listener:pointerdown', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPageWithTimeout('addListenerAndPointerDown()');

      async function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        printEventTargetName(auxData);
        SourcesTestRunner.setEventListenerBreakpoint('listener:pointerdown', false);
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        TestRunner.evaluateInPage('addListenerAndPointerDown()', next);
      }
    });
  }

  SourcesTestRunner.runDebuggerTestSuite(testFunctions);

  function printEventTargetName(auxData) {
    var targetName = auxData && auxData.targetName;
    if (targetName)
      TestRunner.addResult('Event target: ' + targetName);
    else
      TestRunner.addResult('FAIL: No event target name received!');
  }
})();
