// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests cookie pane rendering in Network panel\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  function run() {
    var panel = UI.panels.network;
    var target = panel._networkLogView;
    var types = Common.resourceTypes;

    var requestFoo = SDK.NetworkRequest.create('', '', '', '', '');
    requestFoo.setResourceType(types.XHR);
    requestFoo.setRequestIdForTest('foo');
    requestFoo.setRequestHeaders([{name: 'Cookie', value: 'mycookie=myvalue;myother=myvalue2'}]);
    panel._onRequestSelected({data: requestFoo});
    panel._showRequestPanel();
    panel._networkItemView._selectTab('cookies');

    // Ensure this runs after all Promise.resolve
    setTimeout(() => {
      TestRunner.addResult('--------------------------');
      const value = panel._detailsWidget.element.innerText.split('\n').map(line => line.trim()).join('\n').replace(/\n\n+/g, '\n').trim();
      TestRunner.addResult(value);
      TestRunner.completeTest();
    }, 0);
  }

  self.runtime.loadModulePromise('cookie_table').then(run);
})();
