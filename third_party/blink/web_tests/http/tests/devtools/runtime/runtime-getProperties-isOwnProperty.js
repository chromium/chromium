// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests RemoteObject.getProperties.\n`);
  await TestRunner.evaluateInPagePromise(`
      function A() {
          this.testFoo = "abc";
      }

      function B() {
          this.testBar = "cde";
      }

      B.prototype = new A();
      b = new B();
  `);

  var result = await TestRunner.RuntimeAgent.evaluate('window.b');
  var properties = await TestRunner.RuntimeAgent.getProperties(result.objectId, /* isOwnProperty */ false);

  if (!properties) {
    TestRunner.addResult('Properties do not exist');
    TestRunner.completeTest();
    return;
  }

  for (var property of properties) {
    if (property.name.match(/^test/))
      TestRunner.addResult('property.name=="' + property.name + '" isOwn=="' + property.isOwn + '"');
  }
  TestRunner.completeTest();
})();
