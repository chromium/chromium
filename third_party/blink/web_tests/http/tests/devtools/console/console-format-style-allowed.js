// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
    `Tests that console logging dumps properly styled messages, and that the whole message gets the same style, regardless of multiple %c settings.\n`
  );

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.log('%cColors are awesome.', 'color: blue;');
    console.log('%cSo are fonts!', 'font: 1em Helvetica;');
    console.log('%cAnd borders and margins and paddings!', 'border: 1px solid red; margin: 20px; padding: 10px;');
    console.log('%ctext-* is fine by us!', 'text-decoration: none;');

    console.log('%cDisplay, on the other hand, is bad news.', 'display: none;');
    console.log('%cAnd position too.', 'position: absolute;');
  `);

  ConsoleTestRunner.expandConsoleMessages(onExpanded);

  function onExpanded() {
    ConsoleTestRunner.dumpConsoleMessagesWithStyles();
    TestRunner.completeTest();
  }
})();
