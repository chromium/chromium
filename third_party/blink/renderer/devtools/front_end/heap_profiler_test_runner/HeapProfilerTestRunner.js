// Copyright 2017 The Chromium Authors. All
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview using private properties isn't a Closure violation in tests.
 * @suppress {accessControls}
 */

HeapProfilerTestRunner.createHeapSnapshotMockFactories = function() {
  HeapProfilerTestRunner.createJSHeapSnapshotMockObject = function() {
    return {
      _rootNodeIndex: 0,
      _nodeTypeOffset: 0,
      _nodeNameOffset: 1,
      _nodeEdgeCountOffset: 2,
      _nodeFieldCount: 3,
      _edgeFieldsCount: 3,
      _edgeTypeOffset: 0,
      _edgeNameOffset: 1,
      _edgeToNodeOffset: 2,
      _nodeTypes: ['hidden', 'object'],
      _edgeTypes: ['element', 'property', 'shortcut'],
      _edgeShortcutType: -1,
      _edgeHiddenType: -1,
      _edgeElementType: 0,
      _realNodesLength: 18,
      nodes: new Uint32Array([0, 0, 2, 1, 1, 2, 1, 2, 2, 1, 3, 1, 1, 4, 0, 1, 5, 0]),
      containmentEdges: new Uint32Array([2, 6, 3, 1, 7, 6, 0, 1, 6, 1, 8, 9, 1, 9, 9, 1, 10, 12, 1, 11, 15]),
      strings: ['', 'A', 'B', 'C', 'D', 'E', 'a', 'b', 'ac', 'bc', 'bd', 'ce'],
      _firstEdgeIndexes: new Uint32Array([0, 6, 12, 18, 21, 21, 21]),
      createNode: HeapSnapshotWorker.JSHeapSnapshot.prototype.createNode,
      createEdge: HeapSnapshotWorker.JSHeapSnapshot.prototype.createEdge,
      createRetainingEdge: HeapSnapshotWorker.JSHeapSnapshot.prototype.createRetainingEdge
    };
  };

  HeapProfilerTestRunner.createHeapSnapshotMockRaw = function() {
    return {
      snapshot: {
        meta: {
          node_fields: ['type', 'name', 'id', 'self_size', 'retained_size', 'dominator', 'edge_count'],
          node_types: [['hidden', 'object'], '', '', '', '', '', ''],
          edge_fields: ['type', 'name_or_index', 'to_node'],
          edge_types: [['element', 'property', 'shortcut'], '', ''],
          location_fields: ['object_index', 'script_id', 'line', 'column']
        },

        node_count: 6,
        edge_count: 7
      },

      nodes: [
        0, 0, 1, 0, 20, 0, 2, 1, 1, 2, 2, 2, 0,  2, 1, 2, 3, 3, 8, 0,  2,
        1, 3, 4, 4, 10, 0, 1, 1, 4, 5, 5, 5, 14, 0, 1, 5, 6, 6, 6, 21, 0
      ],

      edges: [1, 6, 7, 1, 7, 14, 0, 1, 14, 1, 8, 21, 1, 9, 21, 1, 10, 28, 1, 11, 35],

      locations: [0, 1, 2, 3, 18, 2, 3, 4],

      strings: ['', 'A', 'B', 'C', 'D', 'E', 'a', 'b', 'ac', 'bc', 'bd', 'ce']
    };
  };

  HeapProfilerTestRunner._postprocessHeapSnapshotMock = function(mock) {
    mock.nodes = new Uint32Array(mock.nodes);
    mock.edges = new Uint32Array(mock.edges);
    return mock;
  };

  HeapProfilerTestRunner.createHeapSnapshotMock = function() {
    return HeapProfilerTestRunner._postprocessHeapSnapshotMock(HeapProfilerTestRunner.createHeapSnapshotMockRaw());
  };

  HeapProfilerTestRunner.createHeapSnapshotMockWithDOM = function() {
    return HeapProfilerTestRunner._postprocessHeapSnapshotMock({
      snapshot: {
        meta: {
          node_fields: ['type', 'name', 'id', 'edge_count'],
          node_types: [['hidden', 'object', 'synthetic'], '', '', ''],
          edge_fields: ['type', 'name_or_index', 'to_node'],
          edge_types: [['element', 'hidden', 'internal'], '', ''],
          location_fields: ['object_index', 'script_id', 'line', 'column']
        },

        node_count: 13,
        edge_count: 13
      },

      nodes: [
        2, 0, 1, 4, 1, 11, 2, 2, 1, 11, 3, 3, 2,  5, 4, 0, 2,  6, 5, 1,  1,  1, 6, 0, 1,  2,
        7, 1, 1, 4, 8, 2,  1, 8, 9, 0,  1, 7, 10, 0, 1, 3, 11, 0, 1, 10, 12, 0, 1, 9, 13, 0
      ],

      edges: [
        0,  1, 4, 0,  2, 8, 0,  3, 12, 0,  4, 16, 0,  1, 20, 0,  2, 24, 0, 1,
        24, 0, 2, 28, 1, 3, 32, 0, 1,  36, 0, 1,  40, 2, 12, 44, 2, 1,  48
      ],

      locations: [0, 2, 1, 1, 6, 2, 2, 2],

      strings: ['', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'M', 'N', 'Window', 'native']
    });
  };

  HeapProfilerTestRunner.HeapNode = function(name, selfSize, type, id) {
    this._type = type || HeapProfilerTestRunner.HeapNode.Type.object;
    this._name = name;
    this._selfSize = selfSize || 0;
    this._builder = null;
    this._edges = {};
    this._edgesCount = 0;
    this._id = id;
  };

  HeapProfilerTestRunner.HeapNode.Type = {
    'hidden': 'hidden',
    'array': 'array',
    'string': 'string',
    'object': 'object',
    'code': 'code',
    'closure': 'closure',
    'regexp': 'regexp',
    'number': 'number',
    'native': 'native',
    'synthetic': 'synthetic',
    'bigint': 'bigint'
  };

  HeapProfilerTestRunner.HeapNode.prototype = {
    linkNode: function(node, type, nameOrIndex) {
      if (!this._builder)
        throw new Error('parent node is not connected to a snapshot');

      if (!node._builder)
        node._setBuilder(this._builder);

      if (nameOrIndex === undefined)
        nameOrIndex = this._edgesCount;

      ++this._edgesCount;

      if (nameOrIndex in this._edges) {
        throw new Error(
            'Can\'t add edge with the same nameOrIndex. nameOrIndex: ' + nameOrIndex +
            ' oldNodeName: ' + this._edges[nameOrIndex]._name + ' newNodeName: ' + node._name);
      }

      this._edges[nameOrIndex] = new HeapProfilerTestRunner.HeapEdge(node, type, nameOrIndex);
    },

    _setBuilder: function(builder) {
      if (this._builder)
        throw new Error('node reusing is prohibited');

      this._builder = builder;
      this._ordinal = this._builder._registerNode(this);
    },

    _serialize: function(rawSnapshot) {
      rawSnapshot.nodes.push(this._builder.lookupNodeType(this._type));
      rawSnapshot.nodes.push(this._builder.lookupOrAddString(this._name));
      rawSnapshot.nodes.push(this._id || this._ordinal * 2 + 1);
      rawSnapshot.nodes.push(this._selfSize);
      rawSnapshot.nodes.push(0);
      rawSnapshot.nodes.push(0);
      rawSnapshot.nodes.push(Object.keys(this._edges).length);

      for (const i in this._edges)
        this._edges[i]._serialize(rawSnapshot);
    }
  };

  HeapProfilerTestRunner.HeapEdge = function(targetNode, type, nameOrIndex) {
    this._targetNode = targetNode;
    this._type = type;
    this._nameOrIndex = nameOrIndex;
  };

  HeapProfilerTestRunner.HeapEdge.prototype = {
    _serialize: function(rawSnapshot) {
      if (!this._targetNode._builder)
        throw new Error('Inconsistent state of node: ' + this._name + ' no builder assigned');

      const builder = this._targetNode._builder;
      rawSnapshot.edges.push(builder.lookupEdgeType(this._type));
      rawSnapshot.edges.push(
          (typeof this._nameOrIndex === 'string' ? builder.lookupOrAddString(this._nameOrIndex) : this._nameOrIndex));
      rawSnapshot.edges.push(this._targetNode._ordinal * builder.nodeFieldsCount);
    }
  };

  HeapProfilerTestRunner.HeapEdge.Type = {
    'context': 'context',
    'element': 'element',
    'property': 'property',
    'internal': 'internal',
    'hidden': 'hidden',
    'shortcut': 'shortcut',
    'weak': 'weak'
  };

  HeapProfilerTestRunner.HeapSnapshotBuilder = function() {
    this._nodes = [];
    this._string2id = {};
    this._strings = [];
    this.nodeFieldsCount = 7;
    this._nodeTypesMap = {};
    this._nodeTypesArray = [];

    for (const nodeType in HeapProfilerTestRunner.HeapNode.Type) {
      this._nodeTypesMap[nodeType] = this._nodeTypesArray.length;
      this._nodeTypesArray.push(nodeType);
    }

    this._edgeTypesMap = {};
    this._edgeTypesArray = [];

    for (const edgeType in HeapProfilerTestRunner.HeapEdge.Type) {
      this._edgeTypesMap[edgeType] = this._edgeTypesArray.length;
      this._edgeTypesArray.push(edgeType);
    }

    this.rootNode = new HeapProfilerTestRunner.HeapNode('root', 0, 'object');
    this.rootNode._setBuilder(this);
  };

  HeapProfilerTestRunner.HeapSnapshotBuilder.prototype = {
    generateSnapshot: function() {
      const rawSnapshot = {
        'snapshot': {
          'meta': {
            'node_fields': ['type', 'name', 'id', 'self_size', 'retained_size', 'dominator', 'edge_count'],
            'node_types': [this._nodeTypesArray, 'string', 'number', 'number', 'number', 'number', 'number'],
            'edge_fields': ['type', 'name_or_index', 'to_node'],
            'edge_types': [this._edgeTypesArray, 'string_or_number', 'node']
          }
        },

        'nodes': [],
        'edges': [],
        'locations': [],
        'strings': []
      };

      for (let i = 0; i < this._nodes.length; ++i)
        this._nodes[i]._serialize(rawSnapshot);

      rawSnapshot.strings = this._strings.slice();
      const meta = rawSnapshot.snapshot.meta;
      rawSnapshot.snapshot.edge_count = rawSnapshot.edges.length / meta.edge_fields.length;
      rawSnapshot.snapshot.node_count = rawSnapshot.nodes.length / meta.node_fields.length;
      return rawSnapshot;
    },

    createJSHeapSnapshot: function() {
      const parsedSnapshot = HeapProfilerTestRunner._postprocessHeapSnapshotMock(this.generateSnapshot());
      return new HeapSnapshotWorker.JSHeapSnapshot(parsedSnapshot, new HeapSnapshotWorker.HeapSnapshotProgress());
    },

    _registerNode: function(node) {
      this._nodes.push(node);
      return this._nodes.length - 1;
    },

    lookupNodeType: function(typeName) {
      if (typeName === undefined)
        throw new Error('wrong node type: ' + typeName);

      if (!(typeName in this._nodeTypesMap))
        throw new Error('wrong node type name: ' + typeName);

      return this._nodeTypesMap[typeName];
    },

    lookupEdgeType: function(typeName) {
      if (!(typeName in this._edgeTypesMap))
        throw new Error('wrong edge type name: ' + typeName);

      return this._edgeTypesMap[typeName];
    },

    lookupOrAddString: function(string) {
      if (string in this._string2id)
        return this._string2id[string];

      this._string2id[string] = this._strings.length;
      this._strings.push(string);
      return this._strings.length - 1;
    }
  };

  HeapProfilerTestRunner.createHeapSnapshot = function(instanceCount, firstId) {
    let seed = 881669;

    function pseudoRandom(limit) {
      seed = seed * 355109 + 153763 >> 2 & 65535;
      return seed % limit;
    }

    const builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
    const rootNode = builder.rootNode;
    const gcRootsNode =
        new HeapProfilerTestRunner.HeapNode('(GC roots)', 0, HeapProfilerTestRunner.HeapNode.Type.synthetic);
    rootNode.linkNode(gcRootsNode, HeapProfilerTestRunner.HeapEdge.Type.element);
    const windowNode = new HeapProfilerTestRunner.HeapNode('Window', 20);
    rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut);
    gcRootsNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.element);

    for (let i = 0; i < instanceCount; ++i) {
      const sizeOfB = pseudoRandom(20) + 1;
      const nodeB = new HeapProfilerTestRunner.HeapNode('B', sizeOfB, undefined, firstId++);
      windowNode.linkNode(nodeB, HeapProfilerTestRunner.HeapEdge.Type.element);
      const sizeOfA = pseudoRandom(50) + 1;
      const nodeA = new HeapProfilerTestRunner.HeapNode('A', sizeOfA, undefined, firstId++);
      nodeB.linkNode(nodeA, HeapProfilerTestRunner.HeapEdge.Type.property, 'a');
      nodeA.linkNode(nodeA, HeapProfilerTestRunner.HeapEdge.Type.property, 'a');
    }

    return builder.generateSnapshot();
  };
};

HeapProfilerTestRunner.createHeapSnapshotMockFactories();

HeapProfilerTestRunner.startProfilerTest = function(callback) {
  TestRunner.addResult('Profiler was enabled.');
  HeapProfilerTestRunner._panelReset = TestRunner.override(UI.panels.heap_profiler, '_reset', function() {}, true);
  TestRunner.addSniffer(UI.panels.heap_profiler, '_addProfileHeader', HeapProfilerTestRunner._profileHeaderAdded, true);
  TestRunner.addSniffer(Profiler.ProfileView.prototype, 'refresh', HeapProfilerTestRunner._profileViewRefresh, true);
  TestRunner.addSniffer(Profiler.HeapSnapshotView.prototype, 'show', HeapProfilerTestRunner._snapshotViewShown, true);

  Profiler.HeapSnapshotContainmentDataGrid.prototype.defaultPopulateCount = function() {
    return 10;
  };

  Profiler.HeapSnapshotConstructorsDataGrid.prototype.defaultPopulateCount = function() {
    return 10;
  };

  Profiler.HeapSnapshotDiffDataGrid.prototype.defaultPopulateCount = function() {
    return 5;
  };

  TestRunner.addResult('Detailed heap profiles were enabled.');
  TestRunner.safeWrap(callback)();
};

HeapProfilerTestRunner.completeProfilerTest = function() {
  TestRunner.addResult('');
  TestRunner.addResult('Profiler was disabled.');
  TestRunner.completeTest();
};

HeapProfilerTestRunner.runHeapSnapshotTestSuite = function(testSuite) {
  const testSuiteTests = testSuite.slice();
  let completeTestStack;

  function runner() {
    if (!testSuiteTests.length) {
      if (completeTestStack)
        TestRunner.addResult('FAIL: test already completed at ' + completeTestStack);

      HeapProfilerTestRunner.completeProfilerTest();
      completeTestStack = new Error().stack;
      return;
    }

    const nextTest = testSuiteTests.shift();
    TestRunner.addResult('');
    TestRunner.addResult(
        'Running: ' +
        /function\s([^(]*)/.exec(nextTest)[1]);
    HeapProfilerTestRunner._panelReset.call(UI.panels.heap_profiler);
    TestRunner.safeWrap(nextTest)(runner, runner);
  }

  HeapProfilerTestRunner.startProfilerTest(runner);
};

HeapProfilerTestRunner.assertColumnContentsEqual = function(reference, actual) {
  const length = Math.min(reference.length, actual.length);

  for (let i = 0; i < length; ++i)
    TestRunner.assertEquals(reference[i], actual[i], 'row ' + i);

  if (reference.length > length)
    TestRunner.addResult('extra rows in reference array:\n' + reference.slice(length).join('\n'));
  else if (actual.length > length)
    TestRunner.addResult('extra rows in actual array:\n' + actual.slice(length).join('\n'));
};

HeapProfilerTestRunner.checkArrayIsSorted = function(contents, sortType, sortOrder) {
  function simpleComparator(a, b) {
    return (a < b ? -1 : (a > b ? 1 : 0));
  }

  function parseSize(size) {
    return parseInt(size.replace(/[\xa0,]/g, ''), 10);
  }

  const extractor = {
    text: function(data) {
      data;
    },

    number: function(data) {
      return parseInt(data, 10);
    },

    size: parseSize,

    name: function(data) {
      return data;
    },

    id: function(data) {
      return parseInt(data, 10);
    }
  }[sortType];

  if (!extractor) {
    TestRunner.addResult('Invalid sort type: ' + sortType);
    return;
  }

  let acceptableComparisonResult;

  if (sortOrder === DataGrid.DataGrid.Order.Ascending) {
    acceptableComparisonResult = -1;
  } else if (sortOrder === DataGrid.DataGrid.Order.Descending) {
    acceptableComparisonResult = 1;
  } else {
    TestRunner.addResult('Invalid sort order: ' + sortOrder);
    return;
  }

  for (let i = 0; i < contents.length - 1; ++i) {
    const a = extractor(contents[i]);
    const b = extractor(contents[i + 1]);
    const result = simpleComparator(a, b);

    if (result !== 0 && result !== acceptableComparisonResult) {
      TestRunner.addResult(
          'Elements ' + i + ' and ' + (i + 1) + ' are out of order: ' + a + ' ' + b + ' (' + sortOrder + ')');
    }
  }
};

HeapProfilerTestRunner.clickColumn = function(column, callback) {
  callback = TestRunner.safeWrap(callback);
  const cell = this._currentGrid()._headerTableHeaders[column.id];

  const event = {
    target: {
      enclosingNodeOrSelfWithNodeName: function() {
        return cell;
      }
    }
  };

  function sortingComplete() {
    HeapProfilerTestRunner._currentGrid().removeEventListener(
        Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, this);
    TestRunner.assertEquals(column.id, this._currentGrid().sortColumnId(), 'unexpected sorting');
    column.sort = this._currentGrid().sortOrder();

    function callCallback() {
      callback(column);
    }

    setTimeout(callCallback, 0);
  }

  HeapProfilerTestRunner._currentGrid().addEventListener(
      Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, this);
  this._currentGrid()._clickInHeaderCell(event);
};

HeapProfilerTestRunner.clickRowAndGetRetainers = function(row, callback) {
  callback = TestRunner.safeWrap(callback);

  const event = {
    target: {
      enclosingNodeOrSelfWithNodeName: function() {
        return row._element;
      },

      selectedNode: row
    }
  };

  this._currentGrid()._mouseDownInDataTable(event);
  const rootNode = HeapProfilerTestRunner.currentProfileView()._retainmentDataGrid.rootNode();
  rootNode.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => callback(rootNode));
};

HeapProfilerTestRunner.clickShowMoreButton = function(buttonName, row, callback) {
  callback = TestRunner.safeWrap(callback);
  const parent = row.parent;
  parent.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => setTimeout(() => callback(parent), 0));
  row[buttonName].click();
};

HeapProfilerTestRunner.columnContents = function(column, row) {
  this._currentGrid().updateVisibleNodes();
  const columnOrdinal = HeapProfilerTestRunner.viewColumns().indexOf(column);
  const result = [];
  const parent = row || this._currentGrid().rootNode();

  for (let node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
    if (!node.selectable)
      continue;

    let content = node.element().children[columnOrdinal];

    if (content.firstElementChild)
      content = content.firstElementChild;

    result.push(content.textContent);
  }

  return result;
};

HeapProfilerTestRunner.countDataRows = function(row, filter) {
  let result = 0;

  filter = filter || function(node) {
    return node.selectable;
  };

  for (let node = row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
    if (filter(node))
      ++result;
  }

  return result;
};

HeapProfilerTestRunner.expandRow = function(row, callback) {
  callback = TestRunner.safeWrap(callback);
  row.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => setTimeout(() => callback(row), 0));

  (function expand() {
    if (row.hasChildren())
      row.expand();
    else
      setTimeout(expand, 0);
  })();
};

HeapProfilerTestRunner.expandRowPromise = function(row) {
  return new Promise(resolve => HeapProfilerTestRunner.expandRow(row, resolve));
};

HeapProfilerTestRunner.findAndExpandGCRoots = function(callback) {
  HeapProfilerTestRunner.findAndExpandRow('(GC roots)', callback);
};

HeapProfilerTestRunner.findAndExpandWindow = function(callback) {
  HeapProfilerTestRunner.findAndExpandRow('Window', callback);
};

HeapProfilerTestRunner.findAndExpandRow = async function(name, callback) {
  const row = HeapProfilerTestRunner.findRow(name);
  TestRunner.assertEquals(true, !!row, `"${name}" row`);
  await HeapProfilerTestRunner.expandRowPromise(row);
  TestRunner.safeWrap(callback)(row);
  return row;
};

HeapProfilerTestRunner.findButtonsNode = function(row, startNode) {
  for (let node = startNode || row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
    if (!node.selectable && node.showNext)
      return node;
  }
  return null;
};

HeapProfilerTestRunner.findRow = function(name, parent) {
  return HeapProfilerTestRunner.findMatchingRow(node => node._name === name, parent);
};

HeapProfilerTestRunner.findMatchingRow = function(matcher, parent) {
  parent = parent || this._currentGrid().rootNode();

  for (let node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
    if (matcher(node))
      return node;
  }

  return null;
};

HeapProfilerTestRunner.switchToView = function(title, callback) {
  return new Promise(resolve => {
    callback = TestRunner.safeWrap(callback);
    const view = UI.panels.heap_profiler.visibleView;
    view._changePerspectiveAndWait(title).then(callback).then(resolve);
    HeapProfilerTestRunner._currentGrid().scrollContainer.style.height = '10000px';
  });
};

HeapProfilerTestRunner.takeAndOpenSnapshot = async function(generator, callback) {
  callback = TestRunner.safeWrap(callback);
  const snapshot = generator();
  const profileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;

  function pushGeneratedSnapshot(reportProgress) {
    if (reportProgress) {
      profileType._reportHeapSnapshotProgress({data: {done: 50, total: 100, finished: false}});
      profileType._reportHeapSnapshotProgress({data: {done: 100, total: 100, finished: true}});
    }
    snapshot.snapshot.typeId = 'HEAP';
    profileType._addHeapSnapshotChunk({data: JSON.stringify(snapshot)});
    return Promise.resolve();
  }

  HeapProfilerTestRunner._takeAndOpenSnapshotCallback = callback;
  TestRunner.override(TestRunner.HeapProfilerAgent, 'takeHeapSnapshot', pushGeneratedSnapshot);
  if (!UI.context.flavor(SDK.HeapProfilerModel))
    await new Promise(resolve => UI.context.addFlavorChangeListener(SDK.HeapProfilerModel, resolve));
  profileType._takeHeapSnapshot();
};

/**
 * @return {!Promise<!Profiler.HeapProfileHeader>}
 */
HeapProfilerTestRunner.takeSnapshotPromise = function() {
  return new Promise(resolve => {
    const heapProfileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;
    heapProfileType.addEventListener(Profiler.HeapSnapshotProfileType.SnapshotReceived, finishHeapSnapshot);
    heapProfileType._takeHeapSnapshot();

    function finishHeapSnapshot() {
      const profiles = heapProfileType.getProfiles();
      if (!profiles.length)
        throw 'FAILED: no profiles found.';
      if (profiles.length > 1)
        throw `FAILED: wrong number of recorded profiles was found. profiles.length = ${profiles.length}`;
      const profile = profiles[0];
      UI.panels.heap_profiler.showProfile(profile);

      const dataGrid = HeapProfilerTestRunner.currentProfileView()._dataGrid;
      dataGrid.addEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);

      function sortingComplete() {
        dataGrid.removeEventListener(
            Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);
        resolve(profile);
      }
    }
  });
};

HeapProfilerTestRunner.viewColumns = function() {
  return HeapProfilerTestRunner._currentGrid()._columnsArray;
};

HeapProfilerTestRunner.currentProfileView = function() {
  return UI.panels.heap_profiler.visibleView;
};

HeapProfilerTestRunner._currentGrid = function() {
  return this.currentProfileView()._dataGrid;
};

HeapProfilerTestRunner._snapshotViewShown = function() {
  if (HeapProfilerTestRunner._takeAndOpenSnapshotCallback) {
    const callback = HeapProfilerTestRunner._takeAndOpenSnapshotCallback;
    HeapProfilerTestRunner._takeAndOpenSnapshotCallback = null;
    const dataGrid = this._dataGrid;

    function sortingComplete() {
      dataGrid.removeEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);
      callback();
    }

    dataGrid.addEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);
  }
};

HeapProfilerTestRunner.showProfileWhenAdded = function(title) {
  HeapProfilerTestRunner._showProfileWhenAdded = title;
  return new Promise(resolve => HeapProfilerTestRunner._waitUntilProfileViewIsShown(title, resolve));
};

HeapProfilerTestRunner._profileHeaderAdded = function(profile) {
  if (HeapProfilerTestRunner._showProfileWhenAdded === profile.title)
    UI.panels.heap_profiler.showProfile(profile);
};

HeapProfilerTestRunner._waitUntilProfileViewIsShown = function(title, callback) {
  callback = TestRunner.safeWrap(callback);
  const profilesPanel = UI.panels.heap_profiler;

  if (profilesPanel.visibleView && profilesPanel.visibleView.profile &&
      profilesPanel.visibleView._profileHeader.title === title)
    callback(profilesPanel.visibleView);
  else
    HeapProfilerTestRunner._waitUntilProfileViewIsShownCallback = {title: title, callback: callback};

};

HeapProfilerTestRunner._profileViewRefresh = function() {
  if (HeapProfilerTestRunner._waitUntilProfileViewIsShownCallback &&
      HeapProfilerTestRunner._waitUntilProfileViewIsShownCallback.title === this._profileHeader.title) {
    const callback = HeapProfilerTestRunner._waitUntilProfileViewIsShownCallback;
    delete HeapProfilerTestRunner._waitUntilProfileViewIsShownCallback;
    callback.callback(this);
  }
};

HeapProfilerTestRunner.startSamplingHeapProfiler = async function() {
  if (!UI.context.flavor(SDK.HeapProfilerModel))
    await new Promise(resolve => UI.context.addFlavorChangeListener(SDK.HeapProfilerModel, resolve));
  Profiler.SamplingHeapProfileType.instance._startRecordingProfile();
};

HeapProfilerTestRunner.stopSamplingHeapProfiler = function() {
  Profiler.SamplingHeapProfileType.instance._stopRecordingProfile();
};
