// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that console warnings are issued for a blocked event listener and that there is no crash when an offending listener is removed by the handler.\n`);
  await TestRunner.evaluateInPagePromise(`
      function eventListenerSuicidal(event)
      {
          event.target.removeEventListener("wheel", eventListenerSuicidal);
      }

      function eventListener1(e)
      {
      }

      function eventListener2(event)
      {
          event.preventDefault();
      }

      function dispatchEvents()
      {
          var target = document.createElement("div");
          document.body.appendChild(target);
          var touches = [new Touch({identifier: 1, clientX: 100, clientY: 100, target: target})];
          var touchEventInit = {
              cancelable: true,
              touches: touches,
              targetTouches: touches,
              changedTouches: touches
          };
          var wheelEvent = new WheelEvent("wheel", { cancelable: true, deltaX: -120, deltaY: 120 });
          var events = [
              new WheelEvent("wheel", { cancelable: true, deltaX: -120, deltaY: 120 }),
              new WheelEvent("wheel", { cancelable: false, deltaX: -120, deltaY: 120 }),
              new MouseEvent("mousemove", { cancelable: true, clientX: 100, clinetY: 100, movementX: 0, movementY: 0 }),
              new TouchEvent("touchstart", touchEventInit),
              new TouchEvent("touchmove", touchEventInit),
          ];

          var eventTypes = ["wheel", "mousemove", "touchstart", "touchmove"];
          for (var type of eventTypes) {
             target.addEventListener(type, eventListener1);
             target.addEventListener(type, eventListener2);
          }
          var deadline = performance.now() + 100;
          while (performance.now() < deadline) {};
          for (var event of events)
              target.dispatchEvent(event);

          // Make sure we don't emit warnings twice, make another pass.
          for (var event of events)
              target.dispatchEvent(event);

          // Make sure we don't crash.
          target = document.createElement("div");
          document.body.appendChild(target);
          target.addEventListener("wheel", eventListenerSuicidal);
          target.dispatchEvent(wheelEvent);
      }
  `);

  const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
  consoleModel.addEventListener(
      SDK.ConsoleModel.Events.MessageAdded, TestRunner.safeWrap(onConsoleMessage));
  step1();

  function step1() {
    TestRunner.mainTarget.logAgent().startViolationsReport([{name: 'blockedEvent', threshold: 30000}]);
    TestRunner.evaluateInPage('dispatchEvents()', step2);
  }

  function step2() {
    TestRunner.mainTarget.logAgent().startViolationsReport([{name: 'blockedEvent', threshold: 0.001}]);
    TestRunner.addResult('There should be no warnings above this line');
    TestRunner.evaluateInPage('dispatchEvents()', () => TestRunner.completeTest());
  }

  function onConsoleMessage(event) {
    var message = event.data;
    var text = message.messageText;
    TestRunner.addResult(text.replace(/ \d+ ms/, ' <number> ms'));
  }
})();
