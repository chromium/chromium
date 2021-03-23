// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests completions prototype chain and scope variables.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function A() {
          this.instanceMember = 1;
          this.member1 = 1;
      }

      A.prototype.aMember = 1;
      A.prototype.shadowedMember = 0;
      A.prototype.__proto__ = null;

      function B() {
          A.call(this);
      }

      B.prototype.bMember = 1;
      B.prototype.ePriorityMember = 2;
      B.prototype.shadowedMember = 1;
      B.prototype.__proto__ = A.prototype;

      function C() {
          B.call(this);
      }

      C.prototype.cMember = 1;
      C.prototype.EPriorityMember = 2;
      C.prototype.shadowedMember = 2;
      C.prototype.__proto__ = B.prototype;

      var objectC = new C();

      let prefixA = 1;
      var prefixB;
      const prefixC = 2;
      var prefixD;
      class prefixFoo {};
  `);

  TestRunner.addResult('Completions for objectC.:');
  let completions = await ObjectUI.javaScriptAutocomplete._completionsForExpression('objectC.', 'e');
  for (var completion of completions)
    TestRunner.addObject(completion);

  TestRunner.addResult('Completions for prefix:');
  completions = await ObjectUI.javaScriptAutocomplete._completionsForExpression('', 'prefix');
  for (var completion of completions)
    TestRunner.addObject(completion);

  TestRunner.completeTest();
})();
