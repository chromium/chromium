// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that XML document contents are logged using the correct case in the console.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.dirxml((new DOMParser()).parseFromString("<MixedCase> Test </MixedCase>", "text/xml"));
    var danglingNode = document.implementation.createDocument("", "books");
    console.dirxml(danglingNode.createElement("Book"));
  `);
  await TestRunner.showPanel('elements');

  // Warm up elements renderer.
  ConsoleTestRunner.expandConsoleMessages(callback);

  async function callback() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
