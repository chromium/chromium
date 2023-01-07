// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that time calculator is updated for both visible and hidden requests.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  var target = UI.panels.network.networkLogView;
  target.resourceCategoryFilterUI.toggleTypeFilter(Common.resourceTypes.XHR.category().title(), false);
  TestRunner.addResult('Clicked \'' + Common.resourceTypes.XHR.name() + '\' button.');
  target.reset();

  function appendRequest(id, type, startTime, endTime) {
    var request = SDK.NetworkRequest.create('', '', '', '', '');
    request.setResourceType(type);
    request.setRequestIdForTest(id);
    request.setIssueTime(startTime);
    request.endTime = endTime;
    TestRunner.networkManager.dispatcher.startNetworkRequest(request);
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

  appendRequest('a', Common.resourceTypes.Script, 1, 2);
  appendRequest('b', Common.resourceTypes.XHR, 3, 4);
  appendRequest('c', Common.resourceTypes.Script, 5, 6);

  TestRunner.completeTest();
})();
