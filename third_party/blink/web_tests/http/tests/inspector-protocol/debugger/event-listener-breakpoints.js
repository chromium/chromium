// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startHTML(`<!DOCTYPE html>
      <input type="button" id="test">
      <video id="video" src="/media/resources/test.ogv"></video>`,
                                 'Tests event listener breakpoints.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.DOM.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.DOMDebugger.enable();

  await session.evaluateAsync(`
    function testElementEventListener() {
      return 0;
    }

    function addListenerAndClick() {
      var element = document.getElementById("test");
      element.addEventListener("click", testElementEventListener, true);
      element.click();
    }

    function addListenerAndAuxclick() {
      var element = document.getElementById("test");
      element.addEventListener("auxclick", testElementEventListener, true);
      element.dispatchEvent(new MouseEvent("auxclick", {button: 1}));
    }

    function addListenerAndPointerDown() {
      var element = document.getElementById("test");
      element.addEventListener("pointerdown", testElementEventListener, true);
      element.dispatchEvent(new PointerEvent("pointerdown"));
    }

    function timerFired() {
      window._onTimerFired?.();
      return 0;
    }

    function addLoadListeners() {
      var xhr = new XMLHttpRequest();
      xhr.onload = loadCallback;
      xhr.onerror = loadCallback;
      xhr.open("GET", "/", true);

      var img = new Image();
      img.onload = sendXHR;
      img.onerror = sendXHR;
      img.src = "foo/bar/dummy";

      function sendXHR() {
        xhr.send();
      }
    }

    function loadCallback() {
      window._onLoadCallback?.();
      return 0;
    }

    function playVideo() {
      var video = document.getElementById("video");
      video.addEventListener("play", onVideoPlay, false);
      window._lastPlayPromise = video.play();
    }

    function onVideoPlay() {
      window._onVideoPlay?.();
      return 0;
    }

    new Promise((resolve, reject) => {
      var video = document.getElementById("video");
      if (video.readyState >= 1) {
        resolve();
      } else {
        video.addEventListener("loadedmetadata", () => resolve(), {once: true});
        video.addEventListener(
            "error", () => reject(new Error("Failed to load test.ogv")),
            {once: true});
      }
    })
    //# sourceURL=event-listener-breakpoints.js
  `);

  function logCallFrames(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const url = (scripts.get(frame.location.scriptId) || frame.url || '')
                      .split('/')
                      .pop();
      testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    }
  }

  function printEventTargetName(auxData) {
    const targetName = auxData && auxData.targetName;
    if (targetName) {
      testRunner.log('Event target: ' + targetName);
    } else {
      testRunner.log('FAIL: No event target name received!');
    }
  }

  let expectPause = false;
  dp.Debugger.onPaused(async ({params}) => {
    if (!expectPause) {
      testRunner.log(`FAIL: Unexpected pause (${params.reason}: ${
          JSON.stringify(params.data)})`);
      await dp.Debugger.resume();
    }
  });

  async function runEventListenerTest(name, breakpoints, triggerExpression,
                                      expectedEventName, expectTarget,
                                      retriggerCallback) {
    testRunner.log('\nRunning: ' + name);
    for (const bp of breakpoints) {
      await dp.DOMDebugger.setEventListenerBreakpoint(bp);
    }

    expectPause = true;
    const pausedPromise = dp.Debugger.oncePaused();
    const evalPromise = session.evaluate(triggerExpression);
    const {params} = await pausedPromise;
    if (params.reason !== 'EventListener' ||
        params.data?.eventName !== expectedEventName) {
      testRunner.log(`FAIL: Unexpected pause payload (${params.reason}: ${
          JSON.stringify(params.data)})`);
    }
    testRunner.log('Script execution paused.');
    logCallFrames(params.callFrames);
    if (expectTarget) {
      printEventTargetName(params.data);
    }

    for (const bp of breakpoints) {
      await dp.DOMDebugger.removeEventListenerBreakpoint(bp);
    }
    expectPause = false;
    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.resume({terminateOnResume: false});
    await resumedPromise;
    testRunner.log('Script execution resumed.');
    await evalPromise;

    await retriggerCallback();
  }

  await runEventListenerTest('testClickBreakpoint',
                             [{eventName: 'click', targetName: '*'}],
                             'addListenerAndClick()', 'listener:click', true,
                             () => session.evaluate('addListenerAndClick()'));

  await runEventListenerTest(
      'testAuxclickBreakpoint', [{eventName: 'auxclick', targetName: '*'}],
      'addListenerAndAuxclick()', 'listener:auxclick', true,
      () => session.evaluate('addListenerAndAuxclick()'));

  testRunner.log('\nRunning: testTimerFiredBreakpoint');
  await dp.EventBreakpoints.setInstrumentationBreakpoint({
    eventName: 'setTimeout.callback',
  });
  expectPause = true;
  const timerPausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(timerFired, 10)');
  const {params: timerPausedParams} = await timerPausedPromise;
  if (timerPausedParams.reason !== 'EventListener' ||
      timerPausedParams.data?.eventName !==
          'instrumentation:setTimeout.callback') {
    testRunner.log(`FAIL: Unexpected pause payload (${
        timerPausedParams.reason}: ${JSON.stringify(timerPausedParams.data)})`);
  }
  testRunner.log('Script execution paused.');
  logCallFrames(timerPausedParams.callFrames);
  await dp.EventBreakpoints.removeInstrumentationBreakpoint({
    eventName: 'setTimeout.callback',
  });
  expectPause = false;
  const timerResumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await timerResumedPromise;
  testRunner.log('Script execution resumed.');
  await session.evaluateAsync(`new Promise(resolve => {
    window._onTimerFired = () => {
      delete window._onTimerFired;
      resolve();
    };
    setTimeout(timerFired, 10);
  })`);

  await runEventListenerTest(
      'testLoadBreakpointOnXHR',
      [
        {eventName: 'load', targetName: 'xmlhttprequest'},
        {eventName: 'error', targetName: 'xmlhttprequest'},
      ],
      'addLoadListeners()', 'listener:load', true,
      () => session.evaluateAsync(`new Promise(resolve => {
        window._onLoadCallback = () => {
          delete window._onLoadCallback;
          resolve();
        };
        addLoadListeners();
      })`));

  await runEventListenerTest('testMediaEventBreakpoint',
                             [
                               {eventName: 'play', targetName: 'audio'},
                               {eventName: 'play', targetName: 'video'},
                             ],
                             'playVideo()', 'listener:play', true,
                             () => session.evaluateAsync(`(async () => {
        await window._lastPlayPromise?.catch(() => {});
        const video = document.getElementById("video");
        video.pause();
        video.currentTime = 0;
        await new Promise(resolve => {
          window._onVideoPlay = () => {
            delete window._onVideoPlay;
            resolve();
          };
          playVideo();
        });
      })()`));

  await runEventListenerTest(
      'testPointerEventBreakpoint',
      [{eventName: 'pointerdown', targetName: '*'}],
      'addListenerAndPointerDown()', 'listener:pointerdown', true,
      () => session.evaluate('addListenerAndPointerDown()'));

  testRunner.completeTest();
});
