// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests to ensure data being passed from outside network to filter results filters properly.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  var totalResourceCount = 3;
  NetworkTestRunner.makeFetch('resources/style.css', {}, ensureAllResources);
  NetworkTestRunner.makeFetch('resources/abe.png', {}, ensureAllResources);
  NetworkTestRunner.makeFetch('resources/abe.png', {}, ensureAllResources);
  NetworkTestRunner.makeFetch('resources/cyrillic.html', {}, ensureAllResources);
  var filterArray = [
    {filterType: Network.NetworkLogView.FilterType.Domain, filterValue: '127.0.0.1'},
    {filterType: Network.NetworkLogView.FilterType.Scheme, filterValue: 'http'}
  ];

  var resourceCount = 0;
  function ensureAllResources() {
    if (++resourceCount >= totalResourceCount)
      checkFilters();
  }

  function checkFilters() {
    for (var filter of filterArray) {
      TestRunner.addResult('');
      TestRunner.addResult('filterType: ' + filter.filterType);
      TestRunner.addResult('filterValue: ' + filter.filterValue);
    }
    TestRunner.addResult('');

    Network.NetworkPanel.revealAndFilter(filterArray);

    var nodes = UI.panels.network._networkLogView.flatNodesList();
    var foundNodesCount = 0;
    for (var i = 0; i < nodes.length; i++) {
      if (!Network.NetworkLogView.isRequestFilteredOut(nodes[i]))
        foundNodesCount++;
    }

    TestRunner.addResult('Found results: ' + foundNodesCount);
    TestRunner.addResult('');
    TestRunner.completeTest();
  }
})();
