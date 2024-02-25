// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests top frame navigation events.\n`);
  await TestRunner.showPanel('resources');

  // Reset resourceTreeModel.
  TestRunner.resourceTreeModel.mainFrame.remove();

  for (var eventName in SDK.ResourceTreeModel.Events)
    TestRunner.resourceTreeModel.addEventListener(
        SDK.ResourceTreeModel.Events[eventName], eventHandler.bind(this, eventName));
  for (var eventName in SDK.SecurityOriginManager.Events)
    TestRunner.securityOriginManager.addEventListener(
        SDK.SecurityOriginManager.Events[eventName], eventHandler.bind(this, eventName));

  function eventHandler(eventName, event) {
    switch (eventName) {
      case 'FrameAdded':
        var frame = event.data;
        TestRunner.addResult(`    ${eventName} : ${frame.id}`);
        break;
      case 'FrameDetached':
        var frame = event.data.frame;
        TestRunner.addResult(`    ${eventName} : ${frame.id}`);
        break;
      case 'FrameNavigated':
        var frame = event.data;
        TestRunner.addResult(`    ${eventName} : ${frame.id} : ${frame.loaderId}`);
        break;
      case 'PrimaryPageChanged':
        var frame = event.data.frame;
        TestRunner.addResult(`    ${eventName} : ${frame.id} : ${frame.loaderId}`);
        break;
      case 'SecurityOriginAdded':
      case 'SecurityOriginRemoved':
        var securityOrigin = event.data;
        TestRunner.addResult(`    ${eventName} : ${securityOrigin}`);
        break;
      case 'MainSecurityOriginChanged':
        var mainSecurityOrigin = event.data['mainSecurityOrigin'];
        var unreachableMainSecurityOrigin = event.data['unreachableMainSecurityOrigin'];
        TestRunner.addResult(`    ${eventName} : ${mainSecurityOrigin} ${unreachableMainSecurityOrigin}`);
        break;
      default:
    }
  }

  // Simulate navigation to new render view: do not attach root frame.
  TestRunner.addResult('Navigating root frame');
  TestRunner.resourceTreeModel.frameNavigated(createFramePayload('root1'));
  TestRunner.addResult('Navigating child frame 1');
  TestRunner.resourceTreeModel.frameAttached('child1', 'root1');
  TestRunner.resourceTreeModel.frameNavigated(createFramePayload('child1', 'root1'));
  TestRunner.addResult('Navigating child frame 1 to a different URL');
  TestRunner.resourceTreeModel.frameNavigated(createFramePayload('child1', 'root1', 'child1-new'));
  TestRunner.addResult('Navigating child frame 2');
  TestRunner.resourceTreeModel.frameAttached('child2', 'root1');
  TestRunner.resourceTreeModel.frameNavigated(createFramePayload('child2', 'root1'));
  TestRunner.addResult('Detaching child frame 1');
  TestRunner.resourceTreeModel.frameDetached('child1');

  TestRunner.addResult('Navigating root frame');
  TestRunner.resourceTreeModel.frameAttached('root2');
  TestRunner.resourceTreeModel.frameNavigated(createFramePayload('root2'));

  TestRunner.addResult('Navigating root frame, unreachable');
  TestRunner.resourceTreeModel.frameAttached('rootUnreachable');
  TestRunner.resourceTreeModel.frameNavigated(createUnreachableFramePayload('rootUnreachable'));

  TestRunner.completeTest();

  function createFramePayload(id, parentId, name) {
    var framePayload = {};
    framePayload.id = id;
    framePayload.parentId = parentId || '';
    framePayload.loaderId = 'loader-' + id;
    framePayload.name = 'frame-' + id;
    framePayload.url = 'http://frame/' + (name || id) + '.html';
    framePayload.securityOrigin = framePayload.url;
    framePayload.mimeType = 'text/html';
    return framePayload;
  }

  function createUnreachableFramePayload(id, parentId, name) {
    var framePayload = {};
    framePayload.id = id;
    framePayload.parentId = parentId || '';
    framePayload.loaderId = 'loader-' + id;
    framePayload.name = 'frame-' + id;
    framePayload.url = 'http://frame/' + (name || id) + '.html';
    framePayload.securityOrigin = '://';
    framePayload.mimeType = 'text/html';
    framePayload.unreachableUrl = framePayload.url;
    return framePayload;
  }
})();
