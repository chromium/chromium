// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that the name of the function invoked as object constructor will be displayed as its type in the front-end. Bug 50063.\n`);
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
      function Parent() { }
      function Child() { }
      Child.prototype = new Parent();
      Child.prototype.constructor = Child;
      var outer = { inner: function() { } };
      console.log(new Parent());
      console.log(new Child());
      console.log(new outer.inner());
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
