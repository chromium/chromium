// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Test JS callstacks in timeline.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');

  var sessionId = '6.23';
  var rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'data': {'sessionId': sessionId, 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'tts': 606543
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
      'args': {},
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
        'frame': '0x2f7b63884000',
        'data': {
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
        'frame': '0x2f7b63884000',
        'data': {
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
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 230125,
      'tts': 1758056
    },
    {
      'args': {},
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
      'ts': 251000,
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
      'ts': 251100,
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
      'ts': 251200,
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
      'ts': 251300,
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
      'ts': 251400,
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_b', 'scriptId': 1}, {'functionName': 'recursive_a', 'scriptId': 1},
            {'functionName': 'recursive_b', 'scriptId': 1}, {'functionName': 'recursive_a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253000,
    },
    {
      'args': {
        'data': {
          'stackTrace': [
            {'functionName': 'recursive_a', 'scriptId': 1}, {'functionName': 'recursive_b', 'scriptId': 1},
            {'functionName': 'recursive_a', 'scriptId': 1}
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253008,
    },
    {
      'args': {
        'data': {
          'stackTrace':
              [{'functionName': 'recursive_b', 'scriptId': 1}, {'functionName': 'recursive_a', 'scriptId': 1}]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253012,
    },
    {
      'args': {'data': {'stackTrace': [{'functionName': 'recursive_a', 'scriptId': 1}]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253014,
    },
    {
      'args': {'data': {'stackTrace': []}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253015,
    },
    {
      'args': {
        'data': {
          'stackTrace':
              [{'functionName': 'recursive_b', 'scriptId': 1}, {'functionName': 'recursive_a', 'scriptId': 1}]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253100,
    },
    {
      'args': {'data': {'stackTrace': [{'functionName': 'recursive_a', 'scriptId': 1}]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253200,
    },
    {
      'args': {'data': {'stackTrace': []}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 253300,
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

  await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents);
  PerformanceTestRunner.mainTrackEvents()
      .filter(function(e) {
        return e.duration;
      })
      .forEach(function(e) {
        TestRunner.addResult(
            e.name + ': ' + e.startTime.toFixed(3) + ' / ' + (e.duration.toFixed(3) || 0) + ' ' +
            (e.args.data && e.args.data.functionName || ''));
      });

  TestRunner.completeTest();
})();
