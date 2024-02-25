// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that logging custom elements uses proper formatting.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
    <foo-bar></foo-bar>
  `);
  await TestRunner.evaluateInPagePromise(`
    function registerElement()
    {
      class ElementProto extends HTMLElement {
        constructor() {
          super();
          console.dir(this);
        }
      }
      customElements.define("foo-bar", ElementProto);
    }
  `);

  ConsoleTestRunner.waitUntilMessageReceived(step1);
  TestRunner.evaluateInPage('registerElement();');

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
