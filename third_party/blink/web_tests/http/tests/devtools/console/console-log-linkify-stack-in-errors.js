// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Test that console.log(new Error().stack) would linkify links in stacks for sourceUrls and sourceMaps Bug 424001.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/stack-with-sourceUrl.js');
  await TestRunner.addScriptTag('resources/stack-with-sourceMap.js');
  await TestRunner.evaluateInPagePromise(`
      function forStack()
      {
          console.log(new Error("line\\nbreak").stack);
      }

      forStack();

      function stack1(errorConstructor, text)
      {
          function stack2()
          {
              console.log(new errorConstructor(text).stack);
          }
          stack2();
      }

      function domError()
      {
          try {
              document.body.removeChild(document.createElement("a"));
          } catch (e) {
              console.log(e.stack);
          }
      }

      domError();

      function logError()
      {
          try {
              throw new Error("some error");
          } catch (e) {
              console.log(e);
          }
      }

      logError();

      console.log("Error message without stacks http://www.chromium.org/");

      console.log("Error valid stack #2\\n    at http://www.chromium.org/boo.js:40:70\\n    at foo(http://www.chromium.org/foo.js:10:50)");
      console.log("Error valid stack #3\\n    at http://www.chromium.org/foo.js:40");
      console.log("Error: MyError\\n    at throwError (http://www.chromium.org/foo.js:40)\\n    at eval (eval at <anonymous> (http://www.chromium.org/foo.js:42:1), <anonymous>:1:1)\\n    at http://www.chromium.org/foo.js:239");

      stack1(ReferenceError, "valid stack");
      stack1(EvalError, "valid stack");
      stack1(SyntaxError, "valid stack");
      stack1(RangeError, "valid stack");
      stack1(TypeError, "valid stack");
      stack1(URIError, "valid stack");

      console.log("Error broken stack\\n    at function_name(foob.js foob.js:30:1)\\n at foob.js:40:70");
      console.log("Error broken stack #2\\n    at function_name(foob.js:20:30");
      console.log("Error broken stack #3\\n    at function_name(foob:20.js:30   bla");
      console.log("Error broken stack #4\\n    at function_name)foob.js:20:30(");
      console.log("Error broken stack #5\\n    at function_name foob.js:20:30)");
      console.log("Error broken stack #6\\n    at foob.js foob.js:40:70");

      //# sourceURL=console-log-linkify-stack-in-errors.js
    `);

  TestRunner.evaluateInPageWithTimeout('failure()');
  ConsoleTestRunner.waitUntilMessageReceived(waitForUISourceCode);
  function waitForUISourceCode() {
    TestRunner.waitForUISourceCode('stack-with-sourceMap.coffee').then(dumpMessages);
  }

  async function dumpMessages() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
