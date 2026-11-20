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

  // Expected: error because a frameless document is not active.
  const {result: {result: inactiveDoc}} = await dp.Runtime.evaluate({
    expression: `document.implementation.createHTMLDocument('')`,
  });
  testRunner.log(await dp.Accessibility.queryAXTree({
    objectId: inactiveDoc.objectId,
    accessibleName: 'name',
  }));

  // Expected: error because backendNodeId from the previous document no longer
  // belongs to an inspected frame after reload.
  await dp.Page.enable();
  const {result: {root}} = await dp.DOM.getDocument({depth: 0});
  const loadPromise = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await loadPromise;
  testRunner.log(await dp.Accessibility.queryAXTree({
    backendNodeId: root.backendNodeId,
    accessibleName: 'name',
  }));

  testRunner.completeTest();
});
