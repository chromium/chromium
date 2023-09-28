// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for DataTransferItem.getAsString.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="text" id="input">
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          window.addEventListener("paste", onPaste, false);

          var input = document.getElementById("input");
          input.value = "value";
          input.focus();
          input.select();
          document.execCommand("Copy");

          input.value = "";
          input.focus();
          document.execCommand("Paste");
      }

      function onPaste(e)
      {
          debugger;
          window.removeEventListener("paste", onPaste, false);
          var items = (e.originalEvent || e).clipboardData.items;
          var item = items[0];
          item.getAsString(onGetAsString);
      }

      function onGetAsString()
      {
          debugger;
      }
  `);

  var totalDebuggerStatements = 2;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
