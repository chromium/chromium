// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';
import * as ObjectUI from 'devtools/ui/legacy/components/object_ui/object_ui.js';

(async function() {
  TestRunner.addResult('Tests that console logging dumps large arrays properly.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    (function onload()
    {
        var a = [];
        for (var i = 0; i < 42; ++i)
            a[i] = i;
        a[100] = 100;
        console.dir(a);

        var b = [];
        for (var i = 0; i < 10; ++i)
          b[i] = undefined;
        console.dir(b);

        var c = [];
        for (var i = 0; i < 10; ++i)
            c[i] = i;
        c[100] = 100;
        console.dir(c);

        var d = [];
        for (var i = 0; i < 405; ++i)
          d[i] = i;
        console.dir(d);

        var e = [];
        for (var i = 0; i < 10; ++i)
            e[i] = i;
        e[123] = 123;
        e[-123] = -123;
        e[3.14] = 3.14;
        e[4294967295] = 4294967295;
        e[4294967296] = 4294967296;
        e[Infinity] = Infinity;
        e[-Infinity] = -Infinity;
        e[NaN] = NaN;
        console.log("%O", e);

        var f = [];
        f[4294967294] = 4294967294;
        for (var i = 20; i >= 0; i -= 2)
            f[i] = i;
        for (var i = 2, n = 33; n--; i *= 2)
            f[i] = i;
        for (var i = 1; i < 20; i += 2)
            f[i] = i;
        console.log("%O", f)

        var g = new Uint8Array(new ArrayBuffer(Math.pow(20, 6) + Math.pow(20, 4) + 3));
        console.dir(g);

    })();
  `);

  ObjectUI.ObjectPropertiesSection.ArrayGroupingTreeElement.bucketThreshold = 20;
  var messages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
  var sections = [];

  for (var i = 0; i < messages.length; ++i) {
    var consoleMessage = messages[i].consoleMessage();
    var element = messages[i].element();
    var node = element.traverseNextNode(element);

    while (node) {
      const section =
          ObjectUI.ObjectPropertiesSection.getObjectPropertiesSectionFrom(node);
      if (section) {
        sections.push(section);
        section.expand();
      }

      node = node.traverseNextNode(element);
    }
  }

  TestRunner.addSniffer(ObjectUI.ObjectPropertiesSection.ArrayGroupingTreeElement.prototype, 'onpopulate', populateCalled, true);
  var populated = false;

  function populateCalled() {
    populated = true;
  }

  TestRunner.deprecatedRunAfterPendingDispatches(expandRecursively);

  function expandRecursively() {
    for (var i = 0; i < sections.length; ++i) {
      var children = sections[i].rootElement().children();

      for (var j = 0; j < children.length; ++j) {
        for (var treeElement = children[j]; treeElement; treeElement = treeElement.traverseNextTreeElement(true, null, true)) {
          if (treeElement.listItemElement.textContent.indexOf('[[Prototype]]') === -1)
            treeElement.expand();
        }
      }
    }

    if (populated)
      TestRunner.deprecatedRunAfterPendingDispatches(completeTest);
    else
      TestRunner.deprecatedRunAfterPendingDispatches(expandRecursively);
  }

  async function completeTest() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
