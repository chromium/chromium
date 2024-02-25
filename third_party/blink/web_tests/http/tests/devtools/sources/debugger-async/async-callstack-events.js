// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for various DOM events.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <video id="video" src="../../../media/resources/test.ogv"></video>
      <p id="content">
      This content will be selected by range.
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
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
  `);

  var totalDebuggerStatements = 4;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
