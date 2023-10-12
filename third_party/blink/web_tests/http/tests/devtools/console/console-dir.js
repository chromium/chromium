// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console logging dumps proper messages.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    console.dir(["test1", "test2"]);
    console.dir(document.childNodes);
    console.dir(document.evaluate("//head", document, null, XPathResult.ANY_TYPE, null));

    // Object with properties containing whitespaces
    var obj = { $foo5_: 0 };
    obj[" a b "] = " a b ";
    obj["c d"] = "c d";
    obj[""] = "";
    obj["  "] = "  ";
    obj["a\\n\\nb\\nc"] = "a\\n\\nb\\nc";
    obj["negZero"] = -0;
    console.dir(obj);

    // This should correctly display information about the function.
    console.dir(function() {});

    // Test function inferred name in prototype constructor.
    var outer = { inner: function() {} };
    console.dir(new outer.inner());

    // Test "No Properties" placeholder.
    console.dir({ __proto__: null });
    console.dir({ foo: { __proto__: null }});
    // Test "No Scopes" placeholder.
    console.dir(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get);

    // Test big typed array: should be no crash or timeout.
    var bigTypedArray = new Uint8Array(new ArrayBuffer(400 * 1000 * 1000));
    bigTypedArray.PASS = "Non-element properties should be displayed.";
    console.dir(bigTypedArray);

    // document.createEvent("Event") has a special property "isTrusted" flagged "LegacyUnforgeable".
    var event = document.createEvent("Event");
    Object.defineProperty(event, "timeStamp", {value: 0})
    console.dir(event);
    //# sourceURL=console-dir.js
  `);

  ConsoleTestRunner.expandConsoleMessages(step1, expandTreeElementFilter);

  function expandTreeElementFilter(treeElement) {
    var name = treeElement.nameElement && treeElement.nameElement.textContent;
    return name === 'foo' || treeElement.title === '<function scope>';
  }

  function step1() {
    ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages, expandTreeElementFilter);
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
