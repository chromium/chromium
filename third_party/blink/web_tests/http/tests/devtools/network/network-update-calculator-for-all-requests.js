// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Network from 'devtools/panels/network/network.js';

(async function() {
  TestRunner.addResult(`Tests that time calculator is updated for both visible and hidden requests.\n`);
  await TestRunner.showPanel('network');

  var target = Network.NetworkPanel.NetworkPanel.instance().networkLogView;
  target.resourceCategoryFilterUI.toggleTypeFilter(Common.ResourceType.resourceTypes.XHR.category().title(), false);
  TestRunner.addResult('Clicked \'' + Common.ResourceType.resourceTypes.XHR.name() + '\' button.');
  target.reset();

  function appendRequest(id, type, startTime, endTime) {
    TestRunner.networkManager.dispatcher.requestWillBeSent({
      requestId: id,
      timestamp: startTime,
      type,
      request: {url: 'http://example.com/'}
    });
    var request = TestRunner.networkManager.requestForId(id);
    request.endTime = endTime;

    target.refresh();

    var isFilteredOut = Network.NetworkLogView.isRequestFilteredOut(
        target.nodeForRequest(request));
    TestRunner.addResult('');
    TestRunner.addResult(
        'Appended request [' + request.requestId() + '] of type \'' + request.resourceType().name() +
        '\' is hidden: ' + isFilteredOut + ' from [' + request.startTime + '] to [' + request.endTime + ']');
    TestRunner.addResult(
        'Timeline: from [' + target.calculator().minimumBoundary() + '] to [' + target.calculator().maximumBoundary() +
        ']');
  }

  appendRequest('a', 'Script', 1, 2);
  appendRequest('b', 'XHR', 3, 4);
  appendRequest('c', 'Script', 5, 6);

  TestRunner.completeTest();
})();
