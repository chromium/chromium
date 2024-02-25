// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
  'Tests errors when finding DOM nodes by accessible name.');

  const wrongObjectId = 'not-a-node';
  const wrongNodeId = -1;

  // Expected: error because no node is specified
  testRunner.log(await dp.Accessibility.queryAXTree({
    accessibleName: 'name',
  }));
  // Expected: error because nodeId is wrong.
  testRunner.log(await dp.Accessibility.queryAXTree({
    nodeId: wrongNodeId,
    accessibleName: 'name',
  }));
  // Expected: error because backendNodeId is wrong.
  testRunner.log(await dp.Accessibility.queryAXTree({
    backendNodeId: wrongNodeId,
    accessibleName: 'name',
  }));

  // Expected: error because object ID is wrong.
  testRunner.log(await dp.Accessibility.queryAXTree({
    objectId: wrongObjectId,
    accessibleName: 'name',
  }));

  testRunner.completeTest();
});
