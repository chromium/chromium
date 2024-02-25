// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  await TestRunner.showPanel('network');

  var panel = Network.NetworkPanel.NetworkPanel.instance();
  var target = panel.networkLogView;
  var types = Common.ResourceType.resourceTypes;

  var requestFoo = SDK.NetworkRequest.NetworkRequest.create('', '', '', '', '');
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
