// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests node xPath construction\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/node-xpath.xhtml');

  ElementsTestRunner.expandElementsTree(dumpNodes.bind(null, ''));
  var doc;

  function dumpNodes(prefix, node) {
    if (!doc) {
      doc = getDocumentElement();
      node = doc;
    }
    dumpNodeData(node, prefix);
    var children = node.children();
    for (var i = 0; children && i < children.length; ++i)
      dumpNodes(prefix + '  ', children[i]);
    if (node === doc)
      TestRunner.completeTest();
  }

  function getDocumentElement() {
    var map = TestRunner.domModel._idToDOMNode;
    for (var id in map) {
      if (map[id].nodeName() === '#document')
        return map[id];
    }

    return null;
  }

  function dumpNodeData(node, prefix) {
    var result = prefix + '\'' + node.nodeName() + '\':\'' + node.nodeValue() + '\' - \'' +
        Elements.DOMPath.xPath(node, true) + '\' - \'' +
        Elements.DOMPath.xPath(node, false) + '\'';
    TestRunner.addResult(result.replace(/\r?\n/g, '\\n'));
  }
})();
