// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that JS object to node resolution still works even if script evals are prohibited by Content-Security-Policy. The test passes if it doesn't crash. Bug 78705.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  var remoteObject = await TestRunner.evaluateInPageRemoteObject('document');
  TestRunner.addResult('didReceiveDocumentObject');
  var nodeId = await TestRunner.DOMAgent.requestNode(remoteObject.objectId);
  TestRunner.addResult('didRequestNode error = ' + (nodeId ? 'null' : 'error'));
  TestRunner.completeTest();
})();
