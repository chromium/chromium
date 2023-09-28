// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks in debugger.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          function innerTestFunction()
          {
              timeout1();
          }
          setTimeout(innerTestFunction, 0);
      }

      function timeout1()
      {
          debugger;
          requestAnimationFrame(animFrame1);
          var id = setInterval(innerInterval1, 0);
          function innerInterval1()
          {
              clearInterval(id);
              interval1();
          }
      }

      function animFrame1()
      {
          console.log('animFrame1');
          debugger;
          setTimeout(timeout2, 0);
          requestAnimationFrame(animFrame2);
      }

      function interval1()
      {
          debugger;
      }

      function timeout2()
      {
          debugger;
      }

      function animFrame2()
      {
          debugger;
          function longTail0()
          {
              timeout3();
          }
          var funcs = [];
          for (var i = 0; i < 20; ++i)
              funcs.push("function longTail" + (i + 1) + "() { setTimeout(longTail" + i + ", 0); };");
          funcs.push("setTimeout(longTail" + i + ", 0);");
          eval(funcs.join("\\n"));
      }

      function timeout3()
      {
          debugger;
      }
  `);

  var totalDebuggerStatements = 6;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
