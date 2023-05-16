// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that resolveNode from alien document does not crash. https://bugs.webkit.org/show_bug.cgi?id=71806.\n`);
  await TestRunner.showPanel('elements');

  var result = await TestRunner.RuntimeAgent.evaluate(
      'var doc = document.implementation.createHTMLDocument(\'\'); doc.lastChild.innerHTML = \'<span></span>\'; doc.lastChild');

  var spanWrapper = TestRunner.runtimeModel.createRemoteObject(result);
  var node = await TestRunner.domModel.pushObjectAsNodeToFrontend(spanWrapper);
  TestRunner.assertTrue(node, 'Node object should be resovled');
  var remoteObject = await node.resolveToObject();
  TestRunner.addResult('Alien node should resolve to null: ' + remoteObject);
  TestRunner.completeTest();
})();
