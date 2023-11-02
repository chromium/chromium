// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test timeline aggregated details.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('timeline');

  const sessionId = '6.23';
  const rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': 'metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': 'metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {
        'data': {
          'page': '0x2f7b63884000',
          'sessionId': sessionId,
          'persistentIds': true,
          'frames': [
            {'frame': '0x2f7b63884000', 'url': 'top-page-url', 'name': 'top-page-name'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'tts': 606543
    },
    {'pid':17851,'tid':23,'ts':101000,'ph':'b','cat':'blink.user_timing','name':'HTML done parsing','args':{},'id':'0x472bc7'},
    {'pid':17851,'tid':23,'ts':102000,'ph':'e','cat':'blink.user_timing','name':'HTML done parsing','args':{},'id':'0x472bc7'},
    {'pid':17851,'tid':23,'ts':101250,'ph':'b','cat':'blink.user_timing','name':'eval scripts','args':{},'id':'0xa09f70'},
    {'pid':17851,'tid':23,'ts':102000,'ph':'e','cat':'blink.user_timing','name':'eval scripts','args':{},'id':'0xa09f70'}
  ];

  const timeline = UI.panels.timeline;
  timeline.setModel(await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents));

  testEventTree('CallTree');
  testEventTree('BottomUp');
  testEventTree('EventLog');
  TestRunner.completeTest();

  function getTreeView(type) {
    timeline.flameChart.detailsView.tabbedPane.selectTab(type, true);
    return timeline.flameChart.detailsView.tabbedPane.visibleView;
  }

  function testEventTree(type) {
    const flameChart = timeline.flameChart.mainFlameChart;
    flameChart.selectGroup(flameChart.rawTimelineData.groups.findIndex(group => group.name === 'Timings'));
    TestRunner.addResult('');
    TestRunner.addResult(type);
    const tree = getTreeView(type);
    const rootNode = tree.dataGrid.rootNode();
    for (const node of rootNode.children)
      printEventTree(1, node.profileNode, node.treeView);
  }

  function printEventTree(padding, node, treeView) {
    let name;
    if (node.isGroupNode()) {
      name = treeView.displayInfoForGroupNode(node).name;
    } else {
      name = node.event.name === TimelineModel.TimelineModel.RecordType.JSFrame ?
          UI.beautifyFunctionName(node.event.args['data']['functionName']) :
          Timeline.TimelineUIUtils.eventTitle(node.event);
    }
    TestRunner.addResult('  '.repeat(padding) + `${name}: ${node.selfTime.toFixed(3)}  ${node.totalTime.toFixed(3)}`);
    node.children().forEach(printEventTree.bind(null, padding + 1));
  }
})();
