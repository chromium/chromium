// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests TestRunner.RuntimeAgent.callFunctionOn usages.\n`);


  var obj1, obj2;

  TestRunner.runTestSuite([
    async function testThis(next) {
      obj1 = await TestRunner.RuntimeAgent.evaluate('({ a : 1, b : 2 })');

      function sum() {
        return this.a + this.b;
      }
      var result = await TestRunner.RuntimeAgent.callFunctionOn(sum.toString(), obj1.objectId);

      TestRunner.addResult(result.value);
      next();
    },

    async function testArguments(next) {
      obj2 = await TestRunner.RuntimeAgent.evaluate('({ c : 1, d : 2 })');

      function format(aobj1, aobj2, val, undef) {
        return JSON.stringify(this) + '\n' + JSON.stringify(aobj1) + '\n' + JSON.stringify(aobj2) + '\n' + val + '\n' +
            undef;
      }
      var result =
          await TestRunner.RuntimeAgent.callFunctionOn(format.toString(), obj1.objectId, [obj1, obj2, {value: 4}, {}]);

      TestRunner.addResult(result.value);
      next();
    }
  ]);
})();
