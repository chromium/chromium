// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that Elements panel allows to change src attribute on iframes inside inspected page. See bug 41350.\n`);

  var messagePromise = new Promise(x => ConsoleTestRunner.addConsoleSniffer(x));
  await TestRunner.loadHTML(`
      <iframe src="resources/iframe-from-different-domain-data.html" id="receiver"></iframe>
    `);
  var message = await messagePromise;
  TestRunner.addResult(message.messageText);

  var node = await new Promise(x => ElementsTestRunner.nodeWithId('receiver', x));
  var messagePromise = new Promise(x => ConsoleTestRunner.addConsoleSniffer(x));
  node.setAttribute('src', 'src="http://localhost:8000/devtools/resources/iframe-from-different-domain-data.html"');
  var message = await messagePromise;
  TestRunner.addResult(message.messageText);
  TestRunner.completeTest();
})();
