// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests inspector cookies table\n`);


  function dumpCookies(cookies) {
    TestRunner.addResult(cookies.map(x => x.name()).join(','));
  }

  function dumpNodeCalls(calls) {
    TestRunner.addResult(`# of node.appendChild calls: ${calls.appendChild.length}`);
    TestRunner.addResult(`# of node.removeChildren calls: ${calls.removeChildren.length}`);
    calls.appendChild.forEach(node => {
      const data = node._data;
      const selected = node._selected ? '[selected]' : '';
      TestRunner.addResult(`node.appendChild call: ${data.name} ${data.value} ${data.expires} ${selected}`.trim());
    });
  }

  function mockNode(options) {
    var calls = {
      appendChild: [],
      removeChildren: [],
    };

    var mockNode = {
      _calls: calls,
      dataGrid: options.dataGrid,
      cookie: options.cookie,
      deselect: () => undefined,
      traverseNextNode: () => options.nextNeighbor,
      traversePreviousNode: () => options.previousNeighbor,
      appendChild: function(node) {
        node.dataGrid = mockNode.dataGrid;
        calls.appendChild.push(node);
      },
      removeChildren: function() {
        if (mockNode.dataGrid) {
          mockNode.dataGrid.selectedNode = null;
        }

        calls.removeChildren.push(arguments);
      },
    };

    return mockNode;
  }

  function mockDataGrid(options) {
    var calls = {
      addCreationNode: [],
      startEditingNextEditableColumnOfDataGridNode: [],
    };

    var dataGrid = {
      rootNode: () => {
        options.rootNode.dataGrid = dataGrid;
        return options.rootNode;
      },
      selectedNode: options.selectedNode,
      isSortOrderAscending: () => options.isAsc,
      sortColumnId: () => options.sortColumn,
      addCreationNode: function() {
        calls.addCreationNode.push(arguments);
      },
      dispatchEventToListeners: () => undefined,
      startEditingNextEditableColumnOfDataGridNode: function() {
        calls.startEditingNextEditableColumnOfDataGridNode.push(arguments);
      },
      _calls: calls,
    };

    return dataGrid;
  }

  function createCookie(data) {
    const cookie = new SDK.Cookie(data.name, data.value);
    for (let key in data) {
      if (key === 'name' || key === 'value')
        continue;

      cookie.addAttribute(key, data[key]);
    }
    return cookie;
  }

  function createSortAndDumpCookies(cookieData, sortColumn, isAsc) {
    const table = new CookieTable.CookiesTable(SDK.targetManager.mainTarget(), true);
    const cookies = cookieData.map(createCookie);
    table._dataGrid = mockDataGrid({sortColumn, isAsc});
    table._sortCookies(cookies);
    TestRunner.addResult(`params: ${sortColumn} ${isAsc ? 'asc' : 'desc'}`);
    dumpCookies(cookies);
  }

  function createBuildAndDumpTable(cookieData, selectedNode, isAsc, lastEditedColumn) {
    const table = new CookieTable.CookiesTable(SDK.targetManager.mainTarget(), true);
    const cookies = cookieData && cookieData.map(createCookie);
    const rootNode = mockNode({});
    table._lastEditedColumnId = lastEditedColumn || null;
    table._dataGrid = mockDataGrid({
      isAsc: Boolean(isAsc),
      sortColumn: 'name',
      rootNode: rootNode,
      selectedNode: selectedNode,
    });
    table.setCookies(cookies);
    dumpNodeCalls(rootNode._calls);
  }

  function testSortCookies(cookieData) {
    createSortAndDumpCookies(cookieData, 'name', false);
    createSortAndDumpCookies(cookieData, 'value', true);
    createSortAndDumpCookies(cookieData, 'path', false);
    createSortAndDumpCookies(cookieData, 'domain', true);
    createSortAndDumpCookies(cookieData, null, true);
  }

  function testRebuildTable(cookieData) {
    TestRunner.addResult('\n--- Empty test ---\n');
    createBuildAndDumpTable([], null, true);
    TestRunner.addResult('\n--- Null test ---\n');
    createBuildAndDumpTable(null, null, true);
    TestRunner.addResult('\n--- Ascending no selection test ---\n');
    createBuildAndDumpTable(cookieData, null, true);
    TestRunner.addResult('\n--- Descending selection test ---\n');
    createBuildAndDumpTable(cookieData, mockNode({cookie: createCookie(cookieData[2])}), false);
    TestRunner.addResult('\n--- Neighbor selection test ---\n');
    createBuildAndDumpTable(cookieData, mockNode({nextNeighbor: {cookie: createCookie(cookieData[0])}}), true);
    TestRunner.addResult('\n--- Previous neighbor selection test ---\n');
    createBuildAndDumpTable(
        cookieData, mockNode({nextNeighbor: {}, previousNeighbor: {cookie: createCookie(cookieData[4])}}), true);
    TestRunner.addResult('\n--- Inactive node test ---\n');
    createBuildAndDumpTable(
        cookieData, mockNode({cookie: createCookie({name: 'missing', value: 'cookie'})}), true, 'name');
  }

  function run() {
    const cookieData = [
      {name: 'cookieA', value: '11', path: '/zzz', domain: 'example.com', expires: 0},
      {name: 'cookieB', value: '2', path: '/abc', domain: '.example.com', expires: 1},
      {name: 'cookieC', value: 'foo', path: '/', domain: 'abc.example.com', expires: 1545057090000},
      {name: 'cookieD', value: '{other}', path: '/aa', domain: '.other.com', expires: 19941213},
      {name: 'cookieE', value: 'zz', path: '/gg', domain: 'z.example.com', expires: 6121598},
      {name: 'cookieF', value: 'null', path: '/', domain: 'example.com', expires: -1},
    ];
    TestRunner.addResult('\n-------- RebuildTable --------\n');
    testRebuildTable(cookieData);
    TestRunner.addResult('\n-------- Sort cookies --------\n');
    testSortCookies(cookieData);
    TestRunner.completeTest();
  }

  self.runtime.loadModulePromise('cookie_table').then(run);
})();
