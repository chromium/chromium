// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test timeline aggregated details.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('timeline');

  TestRunner.addResult('');

  var sessionId = '6.23';
  var rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '__metadata',
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
            {'frame': '0x2f7b63884000', 'url': 'top-page-url', 'name': 'top-page-name'},
            {'frame': '0x2f7b63884100', 'url': 'subframe-url1', 'name': 'subframe-name1', 'parent': '0x2f7b63884000'},
            {'frame': '0x2f7b63884200', 'url': 'about:blank', 'name': 'subframe-name2', 'parent': '0x2f7b63884000'}
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
    {
      'args': {'data': {'frame': '0x2f7b63884300', 'url': 'subframe-url3', 'name': 'subframe-name3', 'parent': '0x2f7b63884000'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'CommitLoad',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100010,
      'tts': 606544
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 200000,
      'tts': 5612442
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'c', 'callUID': 'c', 'scriptId': 1}, {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 208000,
      'tts': 1758056
    },
    {
      'args': {'data': {'frame': '0x2f7b63884100'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 210000,
      'dur': 30000,
      'tts': 5612442
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'c', 'callUID': 'c', 'scriptId': 1}, {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 211000,
      'tts': 1758056
    },
    {
      'args': {'data': {'stackTrace': []}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 212000,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'c', 'callUID': 'c', 'scriptId': 1}, {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 219875,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'frame': '0x2f7b63884000',
          'stackTrace': [
            {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'InvalidateLayout',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 220000,
      'dur': 7000,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'c', 'callUID': 'c', 'scriptId': 1}, {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 220125,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'frame': '0x2f7b63884000',
          'stackTrace': [
            {'functionName': 'b', 'callUID': 'b', 'scriptId': 1},
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'InvalidateLayout',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 221000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'g', 'scriptId': 1}, {'functionName': 'f', 'scriptId': 1},
            {'functionName': 'b', 'scriptId': 1}, {'functionName': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 222000,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'g', 'scriptId': 1}, {'functionName': 'e', 'scriptId': 1},
            {'functionName': 'b', 'scriptId': 1}, {'functionName': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 227125,
      'tts': 1758056
    },
    {
      'name': 'TimeStamp',
      'ts': 227130,
      'ph': 'I',
      'tid': 23,
      'pid': 17851,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo05'}}
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'g', 'scriptId': 1}, {'functionName': 'e', 'scriptId': 1},
            {'functionName': 'b', 'scriptId': 1}, {'functionName': 'a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 227250,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}, {'functionName': 'l', 'callUID': 'l', 'scriptId': 1},
            {'functionName': 'f', 'callUID': 'f', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 230000,
      'tts': 1758056
    },
    {
      'args': {
        'beginData': {
          'frame': '0x2f7b63884200',
          'stackTrace': [
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}, {'functionName': 'l', 'callUID': 'l', 'scriptId': 1},
            {'functionName': 'f', 'callUID': 'f', 'scriptId': 1},
            {'functionName': 'sin', 'callUID': 'sin', 'scriptId': 2, 'url': 'native math.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Layout',
      'ph': 'X',
      'dur': 100,
      'pid': 17851,
      'tid': 23,
      'ts': 230010,
      'tts': 1758056
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}, {'functionName': 'l', 'callUID': 'l', 'scriptId': 1},
            {'functionName': 'f', 'callUID': 'f', 'scriptId': 1},
            {'functionName': 'sin', 'callUID': 'sin', 'scriptId': 2, 'url': 'native math.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TimerInstall',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 230111
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'a', 'callUID': 'a', 'scriptId': 1}, {'functionName': 'l', 'callUID': 'l', 'scriptId': 1},
            {'functionName': 'f', 'callUID': 'f', 'scriptId': 1},
            {'functionName': 'sin', 'callUID': 'sin', 'scriptId': 2, 'url': 'native math.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 230125
    },
    {
      'args': {'data': {'frame': '0x2f7b63884300'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 250000,
      'dur': 10000
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 251000,
      'dur': 1000
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'w', 'callUID': 'w', 'scriptId': 1}, {'functionName': 'z', 'callUID': 'z', 'scriptId': 1},
            {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 251000
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'w', 'callUID': 'w', 'scriptId': 1}, {'functionName': 'z', 'callUID': 'z', 'scriptId': 1},
            {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 251100
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'w', 'scriptId': 1}, {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 251200
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'w', 'scriptId': 1}, {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 251300
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'y', 'callUID': 'y', 'scriptId': 1},
            {'functionName': 'x', 'callUID': 'x', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 251400
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_b', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_b', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253000
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_b', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253008
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_b', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253012
    },
    {
      'args': {
        'data':
            {'stackTrace': [{'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}]}
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253014
    },
    {
      'args': {'data': {'stackTrace': []}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253015
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_b', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'},
            {'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253100
    },
    {
      'args': {
        'data':
            {'stackTrace': [{'functionName': 'recursive_a', 'scriptId': 1, 'url': 'http://www.google.com/rec.js'}]}
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253200
    },
    {
      'args': {'data': {'stackTrace': []}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253300
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 500000,
      'tts': 5612506
    }
  ];

  var timeline = UI.panels.timeline;
  timeline._setModel(PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents));

  var groupByEnum = Timeline.AggregatedTimelineTreeView.GroupBy;
  for (var grouping of Object.values(groupByEnum)) {
    testEventTree('CallTree', grouping);
    testEventTree('BottomUp', grouping);
  }
  testEventTree('EventLog');
  TestRunner.completeTest();

  function getTreeView(type) {
    if (timeline._tabbedPane) {
      timeline._tabbedPane.selectTab(type, true);
      return timeline._flameChart._treeView;
    }
    timeline._flameChart._detailsView._tabbedPane.selectTab(type, true);
    return timeline._flameChart._detailsView._tabbedPane.visibleView;
  }

  function testEventTree(type, grouping) {
    TestRunner.addResult('');
    var tree = getTreeView(type);
    if (grouping) {
      TestRunner.addResult(type + '  Group by: ' + grouping);
      tree._groupBySetting.set(grouping);
    } else {
      TestRunner.addResult(type);
    }
    var rootNode = tree._dataGrid.rootNode();
    for (var node of rootNode.children)
      printEventTree(1, node._profileNode, node._treeView);
  }

  function printEventTree(padding, node, treeView) {
    var name;
    if (node.isGroupNode()) {
      name = treeView._displayInfoForGroupNode(node).name;
    } else {
      name = node.event.name === TimelineModel.TimelineModel.RecordType.JSFrame ?
          UI.beautifyFunctionName(node.event.args['data']['functionName']) :
          Timeline.TimelineUIUtils.eventTitle(node.event);
    }
    TestRunner.addResult('  '.repeat(padding) + `${name}: ${node.selfTime.toFixed(3)}  ${node.totalTime.toFixed(3)}`);
    node.children().forEach(printEventTree.bind(null, padding + 1));
  }
})();
