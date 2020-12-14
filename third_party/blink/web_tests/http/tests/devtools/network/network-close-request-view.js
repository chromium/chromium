// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.showPanel('network');

  var panel = UI.panels.network;
  var target = panel._networkLogView;
  var types = Common.resourceTypes;

  var requestFoo = new SDK.NetworkRequest('', '', '', '', '');
  requestFoo.setResourceType(types.XHR);
  requestFoo.setRequestIdForTest('foo');
  TestRunner.addResult('Showing request foo');
  panel._onRequestSelected({data: requestFoo});
  panel._showRequestPanel();
  TestRunner.addResult('Network Item View: ' + (panel._networkItemView && panel._networkItemView.isShowing()));

  TestRunner.addResult('Hiding request');
  panel._hideRequestPanel();
  TestRunner.addResult('Network Item View: ' + (panel._networkItemView && panel._networkItemView.isShowing()));

  TestRunner.completeTest();
})();