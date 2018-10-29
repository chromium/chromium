// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests profile without hitCounts is parsed correctly.\n`);
  await TestRunner.loadModule('cpu_profiler_test_runner');

  var profile = {
    startTime: 1000,
    endTime: 6000,
    nodes: [
      {id: 1, callFrame: {functionName: '(root)'}, children: [2,3]},
      {id: 2, callFrame: {functionName: '(garbage collector)'}},
      {id: 3, callFrame: {functionName: 'foo'}, children: [4]},
      {id: 4, callFrame: {functionName: 'bar'}}
    ],
    timeDeltas: [500, 250, 1000, 250, 1000],
    samples: [4, 4, 3, 4, 2]
  };

  var model = new SDK.CPUProfileDataModel(profile);
  TestRunner.addResult('Profile tree:');
  printTree('  ', model.profileHead);
  function printTree(padding, node) {
    TestRunner.addResult(
        `${padding}${node.functionName} id:${node.id} total:${node.total} self:${node.self} depth:${node.depth}`);
    node.children.sort((a, b) => a.id - b.id).forEach(printTree.bind(null, padding + '  '));
  }

  TestRunner.addResult('\nsamples: ' + model.samples.join(', '));
  TestRunner.addResult('timestamps: ' + model.timestamps.join(', '));

  TestRunner.completeTest();
})();
