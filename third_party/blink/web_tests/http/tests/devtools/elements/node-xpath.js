// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests node xPath construction\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/node-xpath.xhtml');

  ElementsTestRunner.expandElementsTree(dumpNodes.bind(null, ''));
  var doc;

  function dumpNodes(prefix, node) {
    if (!doc) {
      doc = ElementsTestRunner.getDocumentElement();
      node = doc;
    }
    dumpNodeData(node, prefix);
    var children = node.children();
    for (var i = 0; children && i < children.length; ++i)
      dumpNodes(prefix + '  ', children[i]);
    if (node === doc)
      TestRunner.completeTest();
  }

  function dumpNodeData(node, prefix) {
    var result = prefix + '\'' + node.nodeName() + '\':\'' + node.nodeValue() + '\' - \'' +
        ElementsModule.DOMPath.xPath(node, true) + '\' - \'' +
        ElementsModule.DOMPath.xPath(node, false) + '\'';
    TestRunner.addResult(result.replace(/\r?\n/g, '\\n'));
  }
})();
