// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that highlighting a detached node does not crash. crbug.com/958958\n`);
  await TestRunner.showPanel('elements');

  const remoteObject = await TestRunner.evaluateInPageRemoteObject(`
      var styleElement = document.createElement('style');
      styleElement.type = 'text/css';
      styleElement.textContent = 'content';
      styleElement.id = 'inspected';
      styleElement;
  `);
  const domModel = remoteObject.runtimeModel().target().model(SDK.DOMModel.DOMModel);
  const node = await domModel.pushObjectAsNodeToFrontend(remoteObject);
  node.highlight();

  await TestRunner.OverlayAgent.getHighlightObjectForTest(node.id);
  TestRunner.completeTest();
})();