// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for MutationObserver.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var node = document.createElement("div");
      var nestedNode = document.createElement("div");

      var observer = new MutationObserver(mutationCallback);
      var nestedObserver = new MutationObserver(nestedMutationCallback);

      var timeoutFromMutationCallback;
      var timeoutFromNestedMutationCallback;
      var lastDoMutationsFunc;

      function mutationCallback(mutations)
      {
          if (lastDoMutationsFunc == doMutations1)
              doMutations1(nestedNode);
          debugger;
          if (!timeoutFromMutationCallback)
              timeoutFromMutationCallback = setTimeout(timeoutFromMutation, 0);
      }

      function nestedMutationCallback(mutations)
      {
          debugger;
          if (!timeoutFromNestedMutationCallback)
              timeoutFromNestedMutationCallback = setTimeout(timeoutFromNestedMutation, 0);
      }

      function testFunction()
      {
          setTimeout(timeout1, 0);
          var config = { attributes: true, childList: true, characterData: true };
          observer.observe(node, config);
          nestedObserver.observe(nestedNode, config);
      }

      function timeout1()
      {
          setTimeout(timeout2, 0);
          doMutations1(node);
      }

      function doMutations1(node)
      {
          lastDoMutationsFunc = doMutations1;
          node.setAttribute("foo", "bar");
          node.className = "c1 c2";
          node.textContent = "text";
      }

      function timeout2()
      {
          doMutations2(node);
      }

      function doMutations2(node)
      {
          lastDoMutationsFunc = doMutations2;
          var child = document.createElement("span");
          node.appendChild(child);
          node.textContent = "";
      }

      function timeoutFromMutation()
      {
          debugger;
      }

      function timeoutFromNestedMutation()
      {
          debugger;
          doMutations2(nestedNode);
      }
  `);

  var totalDebuggerStatements = 6;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
