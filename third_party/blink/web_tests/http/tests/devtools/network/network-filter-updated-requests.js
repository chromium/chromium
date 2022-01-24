// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that filter is reapplied when request is updated.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  var target = UI.panels.network.networkLogView;
  var types = Common.resourceTypes;

  var categoryName = types.XHR.category().title();
  target.resourceCategoryFilterUI.toggleTypeFilter(categoryName, false);
  TestRunner.addResult('Clicked \'' + categoryName + '\' button.');

  var requestFoo = SDK.NetworkRequest.create('', '', '', '', '');
  requestFoo.setResourceType(types.Script);
  requestFoo.setRequestIdForTest('foo');
  TestRunner.networkManager.dispatcher.startNetworkRequest(requestFoo);

  var requestBar = SDK.NetworkRequest.create('', '', '', '', '');
  requestBar.setResourceType(types.Script);
  requestBar.setRequestIdForTest('bar');
  TestRunner.networkManager.dispatcher.startNetworkRequest(requestBar);

  async function isFilteredOut(request) {
    var node = await NetworkTestRunner.waitForNetworkLogViewNodeForRequest(request);
    return Network.NetworkLogView.isRequestFilteredOut(node);
  }

  TestRunner.addResult('');
  TestRunner.addResult(
      'Request [' + requestFoo.requestId() + '] of type \'' + requestFoo.resourceType().name() +
      '\' is hidden: ' + await isFilteredOut(requestFoo));
  TestRunner.addResult(
      'Request [' + requestBar.requestId() + '] of type \'' + requestBar.resourceType().name() +
      '\' is hidden: ' + await isFilteredOut(requestBar));

  TestRunner.addResult('');
  requestFoo.setResourceType(types.XHR);
  TestRunner.networkManager.dispatcher.updateNetworkRequest(requestFoo);
  TestRunner.addResult('Updated request [' + requestFoo.requestId() + '] type.');
  TestRunner.networkManager.dispatcher.updateNetworkRequest(requestBar);
  TestRunner.addResult('Updated request [' + requestBar.requestId() + '].');
  target.refresh();

  TestRunner.addResult('');
  TestRunner.addResult(
      'Request [' + requestFoo.requestId() + '] of type \'' + requestFoo.resourceType().name() +
      '\' is hidden: ' + await isFilteredOut(requestFoo));
  TestRunner.addResult(
      'Request [' + requestBar.requestId() + '] of type \'' + requestBar.resourceType().name() +
      '\' is hidden: ' + await isFilteredOut(requestBar));

  TestRunner.completeTest();
})();
