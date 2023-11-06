// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that frame inside crafted frame doesn't cause 'MainFrameNavigated' event and correctly attaches to frame tree. crbug/259036\n`);
  await TestRunner.showPanel('resources');

  var frameId = Symbol('frameId');
  var count = 1;
  var treeModel = TestRunner.resourceTreeModel;
  treeModel.addEventListener(SDK.ResourceTreeModel.Events.FrameAdded, event => {
    var frame = event.data;
    frame[frameId] = ++count;
    log('FrameAdded', frame);
  });
  for (let eventName of ['FrameNavigated', 'FrameDetached', 'MainFrameNavigated'])
    treeModel.addEventListener(SDK.ResourceTreeModel.Events[eventName], event => log(eventName, event.data));

  await TestRunner.evaluateInPagePromise(`
    (function createCraftedIframe() {
      var fabricatedFrame = document.createElement("iframe");
      fabricatedFrame.src = "#foo";
      document.body.appendChild(fabricatedFrame);
      fabricatedFrame.contentDocument.write("<div id='d'></div>");
      fabricatedFrame.contentDocument.getElementById("d").innerHTML = "<iframe src='resources/dummy-iframe.html'></iframe>";
    })();
  `);
  await TestRunner.waitForUISourceCode('dummy-iframe.html');
  TestRunner.completeTest();

  function log(name, frame) {
    var parentFrameId = frame.parentFrame() ? ', parentFrameId: ' + (frame.parentFrame()[frameId] || 1) : '';
    TestRunner.addResult(
        '    ' + name + ' id: ' + frame[frameId] + parentFrameId + ', isMainFrame: ' + frame.isMainFrame());
  }

})();
