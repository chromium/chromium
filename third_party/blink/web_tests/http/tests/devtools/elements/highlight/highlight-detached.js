// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that highlighting a detached node does not crash. crbug.com/958958\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  const remoteObject = await TestRunner.evaluateInPageRemoteObject(`
      var styleElement = document.createElement('style');
      styleElement.type = 'text/css';
      styleElement.textContent = 'content';
      styleElement.id = 'inspected';
      styleElement;
  `);
  const domModel = remoteObject.runtimeModel().target().model(SDK.DOMModel);
  const node = await domModel.pushObjectAsNodeToFrontend(remoteObject);
  node.highlight();

  await TestRunner.OverlayAgent.getHighlightObjectForTest(node.id);
  TestRunner.completeTest();
})();