// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that calling getter on prototype will call it on the object.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     function logObject()
    {
        var A = function() { this.value = 239; }
        A.prototype = {
            constructor: A,
            get foo()
            {
                return this.value;
            }
        }
        var B = function() { A.call(this); }
        B.prototype = {
            constructor: B,
            __proto__: A.prototype
        }
        console.log(new B());
    }
  `);

  TestRunner.evaluateInPage('logObject()', step2);

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(step3);
  }

  function expandTreeElementFilter(treeElement) {
    var name = treeElement.nameElement && treeElement.nameElement.textContent;
    return name === '__proto__';
  }

  function step3() {
    ConsoleTestRunner.expandConsoleMessages(step4, expandTreeElementFilter);
  }

  function step4() {
    ConsoleTestRunner.expandGettersInConsoleMessages(step5);
  }

  async function step5() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
