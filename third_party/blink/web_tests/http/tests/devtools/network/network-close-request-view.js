// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.showPanel('network');

  var panel = UI.panels.network;
  var target = panel.networkLogView;
  var types = Common.resourceTypes;

  var requestFoo = SDK.NetworkRequest.create('', '', '', '', '');
  requestFoo.setResourceType(types.XHR);
  requestFoo.setRequestIdForTest('foo');
  TestRunner.addResult('Showing request foo');
  panel.onRequestSelected({data: requestFoo});
  panel.showRequestPanel();
  TestRunner.addResult('Network Item View: ' + (panel.networkItemView && panel.networkItemView.isShowing()));

  TestRunner.addResult('Hiding request');
  panel.hideRequestPanel();
  TestRunner.addResult('Network Item View: ' + (panel.networkItemView && panel.networkItemView.isShowing()));

  TestRunner.completeTest();
})();
