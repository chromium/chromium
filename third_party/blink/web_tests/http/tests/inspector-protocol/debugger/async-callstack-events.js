// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
        <video id="video" src="/media/resources/test.ogv"></video>
        <p id="content">
        This content will be selected by range.
        </p>
      `,
      'Tests asynchronous call stacks for various DOM events.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
      function testFunction()
      {
          for (var name in window) {
              if (/^doTest[A-Z]/.test(name) && typeof window[name] === "function")
                  window[name]();
          }
      }

      function setSelection(start, end)
      {
          var node = document.getElementById("content").firstChild;
          var range = document.createRange();
          range.setStart(node, start);
          range.setEnd(node, end);
          var selection = window.getSelection();
          selection.removeAllRanges();
          if (start !== end)
              selection.addRange(range);
      }

      function doTestSelectionChange()
      {
          setSelection(0, 0);
          document.addEventListener("selectionchange", onSelectionChange, false);
          setSelection(0, 4);
          setSelection(0, 8);
          setSelection(0, 0);
      }

      function onSelectionChange()
      {
          document.removeEventListener("selectionchange", onSelectionChange, false);
          debugger;
      }

      function doTestHashChange()
      {
          window.addEventListener("hashchange", onHashChange1, false);
          window.addEventListener("hashchange", onHashChange2, true);
          location.hash = location.hash + "x";
      }

      function onHashChange1()
      {
          window.removeEventListener("hashchange", onHashChange1, false);
          debugger;
      }

      function onHashChange2()
      {
          window.removeEventListener("hashchange", onHashChange2, true);
          debugger;
      }

      function doTestMediaEvents()
      {
          var video = document.getElementById("video");
          video.addEventListener("play", onVideoPlay, false);
          video.play();
      }

      function onVideoPlay()
      {
          video.removeEventListener("play", onVideoPlay, false);
          debugger;
      }
      //# sourceURL=async-callstack-events.js
  `);

  async function formatStackTrace(callFrames, asyncStackTrace,
                                  asyncStackTraceId) {
    const lines = ['Call stack:'];
    for (let i = 0; i < callFrames.length; ++i) {
      const f = callFrames[i];
      const url = scripts.get(f.location.scriptId) || f.url;
      const functionName = f.functionName || '(anonymous)';
      lines.push(
          `    ${i}) ${functionName} (${url}:${f.location.lineNumber + 1})`);
    }
    let st = asyncStackTrace;
    let nextId = asyncStackTraceId;
    while (st || nextId) {
      if (!st) {
        const {result} =
            await dp.Debugger.getStackTrace({stackTraceId: nextId});
        st = result.stackTrace;
      }
      lines.push(`    [${st.description || 'Async Call'}]`);
      for (let i = 0; i < st.callFrames.length; ++i) {
        const f = st.callFrames[i];
        const url = scripts.get(f.scriptId) || f.url;
        const functionName = f.functionName || '(anonymous)';
        lines.push(`    ${i}) ${functionName} (${url}:${f.lineNumber + 1})`);
      }
      nextId = st.parentId;
      st = st.parent;
    }
    return lines.join('\n') + '\n';
  }

  const totalDebuggerStatements = 4;
  const callStacksOutput = [];
  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('setTimeout(testFunction, 0)');

  for (let step = 0; step < totalDebuggerStatements; ++step) {
    const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
        await pausedPromise;
    callStacksOutput.push(
        await formatStackTrace(callFrames, asyncStackTrace, asyncStackTraceId));
    pausedPromise =
        step + 1 < totalDebuggerStatements ? dp.Debugger.oncePaused() : null;
    await dp.Debugger.resume();
  }

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
});
