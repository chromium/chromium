
import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
(async function() {
  TestRunner.addResult(`Tests exception message from eval on nested worker context in console contains stack trace.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function startWorker()
      {
          var worker = new Worker("resources/nested-worker.js");
      }
  `);

  TestRunner.addSniffer(SDK.RuntimeModel.RuntimeModel.prototype, 'executionContextCreated', contextCreated);
  TestRunner.evaluateInPage('startWorker()');

  var contexts_still_loading = 2;
  function contextCreated() {
    contexts_still_loading--;
    if (contexts_still_loading > 0) {
      TestRunner.addSniffer(SDK.RuntimeModel.RuntimeModel.prototype, 'executionContextCreated', contextCreated);
      return;
    }

    ConsoleTestRunner.changeExecutionContext('\u2699 worker.js');
    ConsoleTestRunner.evaluateInConsole('\
            function foo()\n\
            {\n\
                throw {a:239};\n\
            }\n\
            function boo()\n\
            {\n\
                foo();\n\
            }\n\
            boo();', step2);
  }

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(step3);
  }

  async function step3() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
