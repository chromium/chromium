// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="rootElement"></div>
      <input type="button" id="test">
    `,
      'Tests framework black-boxing on DOM, XHR and Event breakpoints.');

  await dp.Runtime.enable();
  await dp.DOM.enable();
  await dp.DOMDebugger.enable();
  await dp.Debugger.enable();

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scriptUrls = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scriptUrls.set(params.scriptId, params.url);
  });

  const blackboxPattern = 'framework\\.js$';
  const frameworkRegex = new RegExp(blackboxPattern);
  await dp.Debugger.setBlackboxPatterns({patterns: [blackboxPattern]});

  await session.evaluate(`
      var Framework = {};

      Framework.empty = function()
      {
      };

      Framework.appendChild = function(parent, child)
      {
          parent.appendChild(child);
      };

      Framework.sendXHR = function(url)
      {
          var request = new XMLHttpRequest();
          request.open("GET", url, true);
          try { request.send(); } catch (e) {}
      };

      Framework.addEventListener = function(element, eventType, listener, capture)
      {
          function Framework_eventListener()
          {
              var result = listener ? listener() : void 0;
              return result;
          }

          function Framework_remover()
          {
              element.removeEventListener(eventType, Framework_eventListener, capture);
          }

          element.addEventListener(eventType, Framework_eventListener, capture);
          return Framework_remover;
      };

      Framework.bind = function(func, thisObject, var_args)
      {
          var args = Array.prototype.slice.call(arguments, 2);

          function Framework_bound(var_args)
          {
              return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments)));
          }
          Framework_bound.toString = function()
          {
              return "Framework_bound: " + func;
          };
          return Framework_bound;
      };
      //# sourceURL=framework.js`);

  await session.evaluate(`
      function appendElement(parentId, childId)
      {
          var child = document.createElement("div");
          child.id = childId;
          var parent = document.getElementById(parentId);
          Framework.appendChild(parent, child);
      }

      function sendXHR(url)
      {
          Framework.sendXHR(url);
      }

      function addListenerAndClick(stop)
      {
          function testElementClicked()
          {
              return 0;
          }

          var button = document.getElementById("test");
          var remover = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          if (stop)
              debugger;
          button.click();
          remover();

          remover = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover();

          // Test both handlers together.
          var remover1 = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          var remover2 = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover1();
          remover2();
      }
      //# sourceURL=frameworks-dom-xhr-event-breakpoints-1.js`);

  function logCallFrames(callFrames) {
    testRunner.log('Call stack:');
    callFrames.forEach((frame, index) => {
      const url = scriptUrls.get(frame.location.scriptId) || '';
      const prefix = frameworkRegex.test(url) ? '  * ' : '    ';
      const fileName = url.split('/').pop();
      testRunner.log(`${prefix}${index}) ${frame.functionName} (${fileName}:${
          frame.location.lineNumber + 1})`);
    });
  }

  let acceptingPauses = false;
  let pausedCount = 0;
  let pauseResolver = null;
  const queuedPauses = [];

  dp.Debugger.onPaused(({params}) => {
    if (!acceptingPauses) {
      const topFrame = params.callFrames[0];
      const topName = topFrame ? topFrame.functionName : '(none)';
      testRunner.log(`FAIL: unexpected pause (${params.reason}) in ${topName}`);
      dp.Debugger.resume();
      return;
    }
    pausedCount++;
    if (pauseResolver) {
      const resolve = pauseResolver;
      pauseResolver = null;
      resolve(params);
    } else {
      queuedPauses.push(params);
    }
  });

  function waitForNextPause() {
    if (queuedPauses.length > 0) {
      return Promise.resolve(queuedPauses.shift());
    }
    return new Promise(resolve => {
      pauseResolver = resolve;
    });
  }

  await testRunner.runTestSuite([
    async function testDOMBreakpoint() {
      const {result: {root}} = await dp.DOM.getDocument();
      const {result: {nodeId}} = await dp.DOM.querySelector(
          {nodeId: root.nodeId, selector: '#rootElement'});
      await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: 'subtree-modified'});

      pausedCount = 0;
      acceptingPauses = true;
      const pausedPromise = waitForNextPause();
      const evalPromise =
          session.evaluate(`appendElement('rootElement', 'childElement')`);
      const params = await pausedPromise;
      acceptingPauses = false;

      logCallFrames(params.callFrames);
      testRunner.log(`Paused reason: ${params.reason}`);
      testRunner.log(`Breakpoint node matches #rootElement: ${
          params.data.nodeId === nodeId}`);
      const {result: {node: targetNode}} =
          await dp.DOM.describeNode({nodeId: params.data.targetNodeId});
      testRunner.log(
          `Target node: ${targetNode.localName}#${targetNode.attributes[1]}`);
      testRunner.log(params.data, 'Paused data:', ['nodeId', 'targetNodeId']);
      await dp.DOMDebugger.removeDOMBreakpoint(
          {nodeId, type: 'subtree-modified'});
      await dp.Debugger.resume();
      await evalPromise;
      testRunner.log(`Pause count: ${pausedCount}`);
    },

    async function testXHRBreakpoint() {
      await dp.DOMDebugger.setXHRBreakpoint({url: 'foo'});

      pausedCount = 0;
      acceptingPauses = true;
      const pausedPromise = waitForNextPause();
      const evalPromise =
          session.evaluate(`sendXHR('${testRunner.url('resources/foo?xhr')}')`);
      const params = await pausedPromise;
      acceptingPauses = false;

      logCallFrames(params.callFrames);
      testRunner.log(`Paused reason: ${params.reason}`);
      testRunner.log(params.data, 'Paused data:');
      await dp.DOMDebugger.removeXHRBreakpoint({url: 'foo'});
      await dp.Debugger.resume();
      await evalPromise;
      testRunner.log(`Pause count: ${pausedCount}`);
    },

    async function testEventListenerBreakpoint() {
      await dp.DOMDebugger.setEventListenerBreakpoint(
          {eventName: 'click', targetName: '*'});

      pausedCount = 0;
      acceptingPauses = true;
      const firstPausePromise = waitForNextPause();
      const evalPromise = session.evaluate('addListenerAndClick(false)');

      const firstParams = await firstPausePromise;
      logCallFrames(firstParams.callFrames);
      testRunner.log(`Paused reason: ${firstParams.reason}`);
      testRunner.log(firstParams.data, 'Paused data:');
      testRunner.log('Executing Resume...');
      const secondPausePromise = waitForNextPause();
      await dp.Debugger.resume();

      const secondParams = await secondPausePromise;
      acceptingPauses = false;
      logCallFrames(secondParams.callFrames);
      testRunner.log(`Paused reason: ${secondParams.reason}`);
      testRunner.log(secondParams.data, 'Paused data:');
      testRunner.log('Executing Resume...');
      await dp.Debugger.resume();

      await evalPromise;
      await dp.DOMDebugger.removeEventListenerBreakpoint(
          {eventName: 'click', targetName: '*'});
      testRunner.log(`Pause count: ${pausedCount}`);
    },
  ]);
});
