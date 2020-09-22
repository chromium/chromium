// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests fetch network filters\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  await NetworkTestRunner.clearNetworkCache();

  async function makeFetchPromise(url) {
    return new Promise(resolve => {
      NetworkTestRunner.makeFetch(url, {}, resolve);
    });
  }

  NetworkTestRunner.recordNetwork();

  await makeFetchPromise('resources/style.css');
  await makeFetchPromise('resources/abe.png');
  await makeFetchPromise('resources/abe.png');  // Ensure result is cached.
  await makeFetchPromise('missing/foo.bar');

  const filterChecks = [
    '-.css',
    '-.png',
    'css',
    '',
    '/.*/',
    '/.*\\..*/',
    '/.*\\.png/',
    '/NOTHINGTOMATCH/',
    '//',
    'png',
    '-missing',
    'is:from-cache',
    '-is:from-cache',
  ];

  for (const filterText of filterChecks) {
    TestRunner.addResult('filterText: ' + filterText);
    setNetworkLogFilter(filterText);
    printNetworkLog();
  }
  setNetworkLogFilter('');

  await TestRunner.evaluateInPageAsync(
      `fetch('data:;base64,c2VuZGluZyB0aGlzIHV0Zi04IHN0cmluZyBhcyBhIGJpbmFyeSBtZXNzYWdlLi4u')`);
  await TestRunner.evaluateInPageAsync(
      `fetch(URL.createObjectURL(new Blob(new Uint8Array([1, 2, 3, 4]))))`);
  UI.panels.network._networkLogView._filterChanged(null);

  TestRunner.addResult('hide data URLs unchecked');
  printNetworkLog();

  UI.panels.network._networkLogView._dataURLFilterUI.setChecked(true);
  UI.panels.network._networkLogView._filterChanged(null);
  TestRunner.addResult('hide data URLs checked');
  printNetworkLog();

  TestRunner.completeTest();

  function printNetworkLog() {
    const nodes = UI.panels.network._networkLogView.flatNodesList();
    let foundNodesCount = 0;
    for (let i = 0; i < nodes.length; i++) {
      if (!Network.NetworkLogView.isRequestFilteredOut(nodes[i]))
        foundNodesCount++;
    }

    TestRunner.addResult('Found results: ' + foundNodesCount);
    TestRunner.addResult('');
  }

  /**
   * @param {string} value
   */
  function setNetworkLogFilter(value) {
    UI.panels.network._networkLogView._textFilterUI.setValue(value);
    UI.panels.network._networkLogView._filterChanged(null);  // event not used in this method, so passing null
  }
})();
