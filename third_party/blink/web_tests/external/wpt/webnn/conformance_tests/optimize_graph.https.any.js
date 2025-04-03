// META: title=test WebNN API optimized graph with logical operations
// META: global=window,worker
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils.js
// META: timeout=long

'use strict';

const optimizedgraphTests = [
  {
    'name': 'Lesser + Where: [Less] -> Bool -> [Where]',
    'graph': {
      'inputs': {
        'LesserInputA': {
          'data': [2],
          'descriptor': {shape: [1], dataType: 'float32'}
        },
        'LesserInputB': {
          'data': [1],
          'descriptor': {shape: [1], dataType: 'float32'},
        },
        'whereTrueValue': {
          'data': [2.2],
          'descriptor': {shape: [1], dataType: 'float32'}
        },
        'whereFalseValue': {
          'data': [1.1],
          'descriptor': {shape: [1], dataType: 'float32'},
        }
      },
      'operators': [
        {
          'name': 'lesser',
          'arguments': [{'a': 'LesserInputA'}, {'b': 'LesserInputB'}],
          'outputs': 'lesserOutput'
        },
        {
          'name': 'where',
          'arguments': [{'condition': 'lesserOutput'}, {'trueValue,': 'whereTrueValue'}, {'falseValue': 'whereFalseValue'}],
          'outputs': 'output'
        },
      ],
      'expectedOutputs': {
        'output': {
          'data': [1.1],
          'descriptor': {shape: [1], dataType: 'float32'}
        }
      }
    }
  },
  {
    'name': 'Lesser + Add: [Less] -> Bool -> [Cast] -> Uint8 -> [Add]',
    'graph': {
      'inputs': {
        'LesserInputA': {
          'data': [2],
          'descriptor': {shape: [1], dataType: 'float32'}
        },
        'LesserInputB': {
          'data': [1],
          'descriptor': {shape: [1], dataType: 'float32'},
        },
        'AddInputB': {
          'data': [3],
          'descriptor': {shape: [1], dataType: 'uint8'}
        }
      },
      'operators': [
        {
          'name': 'lesser',
          'arguments': [{'a': 'LesserInputA'}, {'b': 'LesserInputB'}],
          'outputs': 'lesserOutput'
        },
        {
          'name': 'add',
          'arguments': [{'a,': 'lesserOutput'}, {'b': 'AddInputB'}],
          'outputs': 'output'
        },
      ],
      'expectedOutputs': {
        'output': {
          'data': [3],
          'descriptor': {shape: [1], dataType: 'uint8'}
        }
      }
    }
  },
  {
    'name': 'Add + Where: [Add] -> uint8 -> [Cast] -> bool -> [Where]',
    'graph': {
      'inputs': {
        'AddInputA': {
          'data': [2],
          'descriptor': {shape: [1], dataType: 'uint8'}
        },
        'AddInputB': {
          'data': [1],
          'descriptor': {shape: [1], dataType: 'uint8'},
        },
        'whereTrueValue': {
          'data': [2.2],
          'descriptor': {shape: [1], dataType: 'float32'}
        },
        'whereFalseValue': {
          'data': [1.1],
          'descriptor': {shape: [1], dataType: 'float32'},
        }
      },
      'operators': [
        {
          'name': 'add',
          'arguments': [{'a': 'AddInputA'}, {'b': 'AddInputB'}],
          'outputs': 'addOutput'
        },
        {
          'name': 'where',
          'arguments': [{'condition': 'addOutput'}, {'trueValue,': 'whereTrueValue'}, {'falseValue': 'whereFalseValue'}],
          'outputs': 'output'
        },
      ],
      'expectedOutputs': {
        'output': {
          'data': [2.2],
          'descriptor': {shape: [1], dataType: 'float32'}
        }
      }
    }
  },
];

if (navigator.ml) {
  optimizedgraphTests.forEach((test) => {
    webnn_conformance_test(buildAndExecuteGraph, getPrecisionTolerance, test);
  });
} else {
  test(() => assert_implements(navigator.ml, 'missing navigator.ml'));
}
