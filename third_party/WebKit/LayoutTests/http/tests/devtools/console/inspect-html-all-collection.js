// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that HTMLAllCollection properties can be inspected.\n`);
  await TestRunner.showPanel('console');

  var result = await TestRunner.RuntimeAgent.evaluate('document.all', 'console', false);
  if (!result) {
    TestRunner.addResult('FAILED: ' + error);
    TestRunner.completeTest();
    return;
  }
  var htmlAllCollection = TestRunner.runtimeModel.createRemoteObject(result);
  const len = await htmlAllCollection.callFunctionJSON(
      'function(collection) { return this.length + collection.length; }',
      [{objectId: htmlAllCollection.objectId}]);
  if (!len || typeof len !== 'number')
    TestRunner.addResult('FAILED: unexpected document.all.length: ' + len);
  else
    TestRunner.addResult('PASSED: retrieved length of document.all');
  TestRunner.completeTest();
})();
