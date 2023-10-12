// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Test that logging an error in console would linkify relative URLs\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
console.log(\`Error with relative links
    at (foo1.js:10:50)
    at (//foo2.js:10:50)
    at (/foo3.js:10:50)
    at (../foo4.js:10:50)
    at (./foo5.js:10:50)
    at (./bar/foo6.js:10:50)\`);
//# sourceURL=console-linkify-relative-links.js
    `);

    await ConsoleTestRunner.dumpConsoleMessages();
    var consoleView = Console.ConsoleView.ConsoleView.instance();
    var links = consoleView.visibleViewMessages[0].element().querySelectorAll('.console-message-text .devtools-link');
    for (var link of links)
      TestRunner.addResult(`Link: ${link.textContent}, href: ${link.href}`);
    TestRunner.completeTest();
})();
