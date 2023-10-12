// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Test that console.log() would linkify the links. Bug 231074.\n`);

  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.log("www.chromium.org");
    console.log("http://www.chromium.org/");
    console.log("follow http://www.chromium.org/");
    console.log("string", "http://www.chromium.org/");
    console.log(123, "http://www.chromium.org/");
    console.log("http://www.chromium.org/some?v=114:56:57");
    console.log("http://www.example.com/düsseldorf?neighbourhood=Lörick");
    console.log("http://👓.ws");
    console.log("http:/www.example.com/молодец");
    console.log("http://ar.wikipedia.org/wiki/نجيب_محفوظ/");
    console.log("http://example.com/スター・ウォーズ/");
    console.log("data:text/plain;a");
    console.log("'www.chromium.org'");
    console.log("(www.chromium.org)");
    console.log("\\"www.chromium.org\\"");
    console.log("{www.chromium.org}");
    console.log("[www.chromium.org]");
    console.log("www.chromium.org\\u00a0");
    console.log("www.chromium.org~");
    console.log("www.chromium.org,");
    console.log("www.chromium.org:");
    console.log("www.chromium.org;");
    console.log("www.chromium.org.");
    console.log("www.chromium.org...");
    console.log("www.chromium.org!");
    console.log("www.chromium.org?");
    console.log("at triggerError (http://localhost/show/:22:11)");
  `);

  TestRunner.addResult('Dump urls in messages');
  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewMessages = consoleView.visibleViewMessages;
  for (var i = 0; i < viewMessages.length; ++i) {
    var uiMessage = viewMessages[i];
    var element = uiMessage.element().querySelector('.console-message-text');
    TestRunner.addResult('\nText: ' + element.deepTextContent());
    var links = element.querySelectorAll('.devtools-link');
    for (var link of links) {
      var info = Components.Linkifier.linkInfo(link);
      if (info && info.url) {
        TestRunner.addResult('Linked url: ' + info.url);
        if (info.lineNumber !== null || info.columnNumber !== null)
          TestRunner.addResult(`Line: ${info.lineNumber}, Column: ${info.columnNumber}`);
      }
    }
  }

  // Ensures urls with lots of slashes does not bog down the regex.
  const dummyMessage = viewMessages[0];
  Console.ConsoleViewMessage.ConsoleViewMessage.prototype.linkifyStringAsFragment.call(dummyMessage, '/'.repeat(1000));
  Console.ConsoleViewMessage.ConsoleViewMessage.prototype.linkifyStringAsFragment.call(dummyMessage, '/a/'.repeat(1000));

  TestRunner.completeTest();
})();
