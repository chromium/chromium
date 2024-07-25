// META: title=test WebNN API reduction operations
// META: global=window,dedicatedworker
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils.js
// META: timeout=long

'use strict';

// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce
// Reduce the input tensor along all dimensions, or along the axes specified in
// the axes array parameter.
//
// dictionary MLReduceOptions {
//   sequence<[EnforceRange] unsigned long> axes;
//   boolean keepDimensions = false;
// };
//
// MLOperand reduceL1(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceL2(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceLogSum(
//     MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceLogSumExp(
//     MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceMax(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceMean(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceMin(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceProduct(
//     MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceSum(MLOperand input, optional MLReduceOptions options = {});
// MLOperand reduceSumSquare(
//     MLOperand input, optional MLReduceOptions options = {});


const getReductionOperatorsPrecisionTolerance = (graphResources) => {
  const operatorName = graphResources.operators[0].name;
  const args = graphResources.operators[0].arguments;
  const inputShape = graphResources.inputs[args[0][Object.keys(args[0])[0]]]
                         .descriptor.dimensions;
  const rank = inputShape.length;
  const options =
      args.length === 2 ? {...args[1][Object.keys(args[1])[0]]} : {};
  let sizes;

  if (options && options.axes) {
    sizes = options.axes.map(
        (axis) => axis < 0 ? inputShape[axis + rank] : inputShape[axis]);
  } else {
    sizes = inputShape;
  }

  const reducedElementCount = sizes.length ?
      sizes.reduce((accumulator, currentValue) => accumulator * currentValue) :
      1;
  let tolerance;

  switch (operatorName) {
    case 'reduceL1':
    case 'reduceProduct':
    case 'reduceSum':
      tolerance = reducedElementCount;
      break;
    case 'reduceL2':
      tolerance = reducedElementCount * 2 + 1;
      break;
    case 'reduceMean':
      tolerance = reducedElementCount + 2;
      break;
    case 'reduceLogSum':
      tolerance = reducedElementCount + 18;
      break;
    case 'reduceLogSumExp':
      tolerance = reducedElementCount * 2 + 18;
      break;
    case 'reduceSumSquare':
      tolerance = reducedElementCount * 2;
      break;
    default:
      break;
  }

  const toleranceValueDict = {
    reduceL1: {float32: tolerance, float16: tolerance},
    reduceL2: {float32: tolerance, float16: tolerance},
    reduceLogSum: {float32: tolerance, float16: tolerance},
    reduceLogSumExp: {float32: tolerance, float16: tolerance},
    reduceMax: {float32: 0, float16: 0},
    reduceMean: {float32: tolerance, float16: tolerance},
    reduceMin: {float32: 0, float16: 0},
    reduceProduct: {float32: tolerance, float16: tolerance},
    reduceSum: {float32: tolerance, float16: tolerance},
    reduceSumSquare: {float32: tolerance, float16: tolerance},
  };

  const expectedDataType =
      getExpectedDataTypeOfSingleOutput(graphResources.expectedOutputs);
  return {
    metricType: 'ULP',
    value: toleranceValueDict[operatorName][expectedDataType]
  };
};

const reductionOperatorsTests = [
  // reduceL1 tests
  {
    'name': 'reduceL1 float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [5.50882625579834],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 5.50882625579834,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [5.50882625579834],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}, {'options': {'axes': []}}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 5.50882625579834,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            -98.83928680419922,  -57.66743850708008,  -57.101200103759766,
            -6.693042278289795,  -45.30584716796875,  -86.68338775634766,
            -74.71875,           -76.46739959716797,  -75.37677001953125,
            -18.22093963623047,  -54.64426803588867,  -36.45240020751953,
            -18.322681427001953, -47.94379425048828,  -40.19978332519531,
            -15.830483436584473, -48.883358001708984, -41.600242614746094,
            -20.6556339263916,   -92.2993392944336,   -46.28858184814453,
            -80.57186126708984,  -25.49472999572754,  -48.96730041503906
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1215.228515625,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            18, 29, 35, 36, 4,  76, 41, 18, 53, 29, 25, 94,
            26, 1,  3,  68, 39, 25, 87, 30, 39, 75, 76, 66
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 993,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            -92, -52, -88, -78, -20, -73, -42, -57, -39, -75, -17, -36,
            -81, -24, -88, -91, -76, -5,  -44, -66, -96, -8,  -69, -27
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1344,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [{'input': 'reduceL1Input'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [
            142.01541137695312, 106.62430572509766, 175.39280700683594,
            286.7269592285156, 169.36322021484375, 212.59750366210938
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments':
            [{'input': 'reduceL1Input'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [
            258.57110595703125, 174.42807006835938, 102.19830322265625,
            134.52191162109375, 207.92910766601562, 215.07168579101562
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [
          {'input': 'reduceL1Input'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments':
            [{'input': 'reduceL1Input'}, {'options': {'keepDimensions': true}}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [1092.72021484375],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [
          {'input': 'reduceL1Input'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': 1092.72021484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL1 float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments':
            [{'input': 'reduceL1Input'}, {'options': {'keepDimensions': true}}],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [1092.72021484375],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceL1 float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [
          {'input': 'reduceL1Input'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [
            108.43173217773438, 315.6007995605469, 359.5506591796875,
            309.13702392578125
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceL1 float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL1Input': {
          'data': [
            5.50882625579834,   50.61575698852539,  1.6773051023483276,
            84.2135238647461,   15.664374351501465, 52.89714813232422,
            9.125157356262207,  28.937623977661133, 12.567061424255371,
            11.39999008178711,  86.91246032714844,  64.51329803466797,
            71.2834243774414,   76.34410858154297,  41.53409194946289,
            97.5653305053711,   31.803831100463867, 6.089754581451416,
            61.70843505859375,  69.76119232177734,  38.919403076171875,
            52.288333892822266, 22.31783676147461,  99.0719223022461
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL1',
        'arguments': [
          {'input': 'reduceL1Input'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceL1Output'
      }],
      'expectedOutputs': {
        'reduceL1Output': {
          'data': [
            108.43173217773438, 315.6007995605469, 359.5506591796875,
            309.13702392578125
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceL2 tests
  {
    'name': 'reduceL2 float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [4.860228061676025],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 4.860228061676025,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [4.860228061676025],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}, {'options': {'axes': []}}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 4.860228061676025,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            -66.80043029785156,  -53.00004959106445,  -59.58587646484375,
            -46.14392852783203,  -49.60614013671875,  -12.832738876342773,
            -88.05061340332031,  -75.56246185302734,  -50.76777648925781,
            -36.96630096435547,  -26.344043731689453, -58.90546417236328,
            -94.28752899169922,  -22.7802791595459,   -84.3487777709961,
            -60.47734451293945,  -41.455806732177734, -92.84781646728516,
            -85.05448913574219,  -30.235260009765625, -47.33808135986328,
            -25.268428802490234, -78.11959075927734,  -28.330944061279297
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 292.57574462890625,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4, 29, 8,  56, 42, 78, 89, 64, 56, 81, 85, 18,
            6, 39, 35, 63, 87, 50, 81, 89, 5,  8,  37, 37
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 274.4029846191406,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            -70, -78, -65, -77, -25, -47, -63, -67, -66, -15, -28, -75,
            -88, -54, -13, -27, -5,  -18, -68, -71, -50, -56, -99, -99
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 300.3830871582031,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [{'input': 'reduceL2Input'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [
            122.352783203125, 124.8213119506836, 128.20062255859375,
            128.14801025390625, 87.18083953857422, 55.043975830078125
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments':
            [{'input': 'reduceL2Input'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [
            114.44775390625, 110.26422882080078, 133.47344970703125,
            64.96752166748047, 128.0914764404297, 101.677734375
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [
          {'input': 'reduceL2Input'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments':
            [{'input': 'reduceL2Input'}, {'options': {'keepDimensions': true}}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [272.0996398925781],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [
          {'input': 'reduceL2Input'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': 272.0996398925781,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceL2 float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments':
            [{'input': 'reduceL2Input'}, {'options': {'keepDimensions': true}}],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [272.0996398925781],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceL2 float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [
          {'input': 'reduceL2Input'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [
            138.580078125, 166.67791748046875, 149.91552734375, 67.6578598022461
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceL2 float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceL2Input': {
          'data': [
            4.860228061676025,  88.23184204101562,  54.489688873291016,
            64.75027465820312,  6.855991363525391,  91.39871215820312,
            41.88857650756836,  73.65444946289062,  35.31573486328125,
            48.345428466796875, 82.39190673828125,  77.86200714111328,
            93.31141662597656,  62.48688507080078,  60.29290008544922,
            13.230599403381348, 20.535987854003906, 53.45161819458008,
            11.320085525512695, 64.75763702392578,  43.6589469909668,
            0.8374307155609131, 0.6848266124725342, 33.504703521728516
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceL2',
        'arguments': [
          {'input': 'reduceL2Input'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceL2Output'
      }],
      'expectedOutputs': {
        'reduceL2Output': {
          'data': [
            138.580078125, 166.67791748046875, 149.91552734375, 67.6578598022461
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceLogSum tests
  {
    'name': 'reduceLogSum float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [64.54827117919922],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 4.167413234710693,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [64.54827117919922],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments':
            [{'input': 'reduceLogSumInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 4.167413234710693,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSum float32 1D constant tensor all non-negative default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 1D tensor all non-negative default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSum float32 1D tensor all non-negative integers default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            63, 82, 49, 23, 98, 67, 15, 9,  89, 7, 69, 61,
            47, 50, 41, 39, 58, 52, 35, 83, 81, 7, 34, 9
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.063048362731934,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [{'input': 'reduceLogSumInput'}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments':
            [{'input': 'reduceLogSumInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [
            5.593751907348633, 4.773046016693115, 5.3115739822387695,
            5.2497639656066895, 4.973392486572266, 5.373587131500244
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments':
            [{'input': 'reduceLogSumInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [
            5.410027980804443, 5.367736339569092, 5.399682998657227,
            4.652334213256836, 4.744638442993164, 5.565346717834473
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [7.039101600646973],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': 7.039101600646973,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSum float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [7.039101600646973],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSum float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [
            5.7273993492126465, 5.64375114440918, 5.453810214996338,
            5.758983135223389
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSum float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumInput': {
          'data': [
            64.54827117919922,  97.87423706054688,  26.529027938842773,
            79.79046630859375,  50.394989013671875, 14.578407287597656,
            20.866817474365234, 32.43873596191406,  64.91233825683594,
            71.54029846191406,  11.137068748474121, 55.079307556152344,
            43.791351318359375, 13.831947326660156, 97.39019775390625,
            35.507755279541016, 52.27586364746094,  82.83865356445312,
            8.568099021911621,  0.8337112069129944, 69.23146057128906,
            3.8541641235351562, 70.5567398071289,   71.99264526367188
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSum',
        'arguments': [
          {'input': 'reduceLogSumInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumOutput': {
          'data': [
            5.7273993492126465, 5.64375114440918, 5.453810214996338,
            5.758983135223389
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceLogSumExp tests
  {
    'name': 'reduceLogSumExp float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [0.7974132895469666],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 0.7974132895469666,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [0.7974132895469666],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments':
            [{'input': 'reduceLogSumExpInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 0.7974132895469666,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSumExp float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            -4.025670051574707,  -9.444348335266113,   -3.1193981170654297,
            -5.943697929382324,  -0.3701804578304291,  -4.397126197814941,
            -6.605968475341797,  -5.534277439117432,   -7.361471176147461,
            -1.9987547397613525, -9.093968391418457,   -8.693618774414062,
            -8.416788101196289,  -1.010741114616394,   -9.814584732055664,
            -9.725259780883789,  -9.157071113586426,   -0.001698818989098072,
            -9.963415145874023,  -5.991659641265869,   -6.180599689483643,
            -1.2336505651474,    -0.44234341382980347, -6.990072250366211
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 1.1666961908340454,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSumExp float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            1, 5, 7, 5, 7, 5, 4, 2, 1, 5, 8, 2,
            4, 1, 4, 5, 4, 8, 6, 2, 7, 7, 8, 5
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 9.607237815856934,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSumExp float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            -6, -3, -5,  -1,  -9, -5, -1, -2, -10, -1, -5, -7,
            -7, -3, -10, -10, -8, -6, -2, -6, -1,  -9, -5, -2
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 0.7001367211341858,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [{'input': 'reduceLogSumExpInput'}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments':
            [{'input': 'reduceLogSumExpInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [
            8.55212688446045, 3.985233783721924, 5.52872896194458,
            9.081488609313965, 6.996237754821777, 9.759706497192383
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments':
            [{'input': 'reduceLogSumExpInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [
            4.66951847076416, 9.08117961883545, 8.533217430114746,
            9.270560264587402, 6.450263977050781, 8.917200088500977
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [10.39477825164795],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': 10.39477825164795,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceLogSumExp float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [10.39477825164795],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSumExp float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [
            8.563796997070312, 5.500619411468506, 9.753945350646973,
            9.20864486694336
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceLogSumExp float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceLogSumExpInput': {
          'data': [
            0.7974132895469666,  5.046889781951904,   8.520371437072754,
            1.4063042402267456,  0.11882661283016205, 0.2858544886112213,
            1.9325640201568604,  3.7939958572387695,  2.6040232181549072,
            4.937509536743164,   4.571482181549072,   0.786512017250061,
            0.21018670499324799, 9.063042640686035,   4.099809646606445,
            4.596248626708984,   0.2549232244491577,  1.159480094909668,
            6.802876949310303,   5.234325408935547,   8.914905548095703,
            9.166799545288086,   5.717507362365723,   0.3255050778388977
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceLogSumExp',
        'arguments': [
          {'input': 'reduceLogSumExpInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceLogSumExpOutput'
      }],
      'expectedOutputs': {
        'reduceLogSumExpOutput': {
          'data': [
            8.563796997070312, 5.500619411468506, 9.753945350646973,
            9.20864486694336
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceMax tests
  {
    'name': 'reduceMax float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [32.16658401489258],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 32.16658401489258,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [32.16658401489258],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 32.16658401489258,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 1D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 1D tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [{'input': 'reduceMaxInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [
            90.42288208007812, 75.90379333496094, 94.99645233154297,
            96.55397033691406, 99.77313232421875, 20.253753662109375
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments':
            [{'input': 'reduceMaxInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [
            62.504642486572266, 96.55397033691406, 99.77313232421875,
            -21.557384490966797, 94.99645233154297, 37.28493118286133
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [99.77313232421875],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': 99.77313232421875,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMax float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [99.77313232421875],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMax float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [
            90.42288208007812, 94.99645233154297, 96.55397033691406,
            99.77313232421875
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMax float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMaxInput': {
          'data': [
            32.16658401489258,   90.42288208007812,  -26.341794967651367,
            -7.147959232330322,  75.90379333496094,  -48.2042121887207,
            -53.09425354003906,  66.66099548339844,  -96.16854095458984,
            -88.30545043945312,  94.99645233154297,  37.28493118286133,
            -42.209861755371094, 96.55397033691406,  0.8807229995727539,
            62.504642486572266,  36.650634765625,    99.77313232421875,
            -72.86485290527344,  -46.03200912475586, 20.253753662109375,
            -21.557384490966797, -51.28727340698242, -42.58832931518555
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMax',
        'arguments': [
          {'input': 'reduceMaxInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceMaxOutput'
      }],
      'expectedOutputs': {
        'reduceMaxOutput': {
          'data': [
            90.42288208007812, 94.99645233154297, 96.55397033691406,
            99.77313232421875
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceMean tests
  {
    'name': 'reduceMean float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [95.84498596191406],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 95.84498596191406,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [95.84498596191406],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 95.84498596191406,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMean float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            -37.14686965942383,  -44.500423431396484, -6.1265482902526855,
            -6.321793079376221,  -76.53897857666016,  -4.137692928314209,
            -20.76356315612793,  -38.749176025390625, -36.81039810180664,
            -26.274377822875977, -12.566819190979004, -55.28200912475586,
            -20.69756507873535,  -34.19586181640625,  -45.36003112792969,
            -34.996192932128906, -67.84308624267578,  -0.7434244155883789,
            -21.981258392333984, -61.31269454956055,  -58.598960876464844,
            -76.02980041503906,  -23.91740608215332,  -22.94187355041504
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': -34.74319839477539,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMean float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            42, 24, 44, 38, 82, 93, 64, 40, 48, 78, 81, 59,
            45, 18, 3,  77, 60, 19, 66, 8,  21, 19, 62, 71
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 48.41666793823242,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMean float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            -73, -8,  -55, -73, -61, -54, -5,  -39, -66, -53, -57, -39,
            -62, -98, -36, -1,  -75, -8,  -71, -72, -67, -16, -21, -31
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': -47.54166793823242,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [{'input': 'reduceMeanInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [
            45.46687316894531, 49.670711517333984, 50.94768142700195,
            44.734745025634766, 27.777833938598633, 23.264999389648438
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments':
            [{'input': 'reduceMeanInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [
            54.82453536987305, 40.251548767089844, 22.060470581054688,
            48.58541488647461, 51.343353271484375, 24.797523498535156
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [40.31047439575195],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': 40.31047439575195,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMean float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [40.31047439575195],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMean float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [
            52.287559509277344, 45.10261917114258, 47.640018463134766,
            16.211700439453125
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMean float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMeanInput': {
          'data': [
            95.84498596191406,  75.6937026977539,   1.5417721271514893,
            8.787034034729004,  70.08280181884766,  13.784331321716309,
            20.006067276000977, 94.80963897705078,  25.82918930053711,
            94.13260650634766,  67.72958374023438,  16.09935188293457,
            92.1943359375,      11.567352294921875, 52.70549774169922,
            22.471792221069336, 3.662332534790039,  20.210277557373047,
            58.56523132324219,  28.673492431640625, 42.13419723510742,
            21.63775062561035,  14.160697937011719, 15.127351760864258
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMean',
        'arguments': [
          {'input': 'reduceMeanInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceMeanOutput'
      }],
      'expectedOutputs': {
        'reduceMeanOutput': {
          'data': [
            52.287559509277344, 45.10261917114258, 47.640018463134766,
            16.211700439453125
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceMin tests
  {
    'name': 'reduceMin float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [-58.76195526123047],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -58.76195526123047,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [-58.76195526123047],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -58.76195526123047,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 1D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 1D tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [{'input': 'reduceMinInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [
            -87.9623031616211, -39.50931167602539, -53.747413635253906,
            -31.713542938232422, -84.4076919555664, -55.97655487060547
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments':
            [{'input': 'reduceMinInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [
            -58.76195526123047, -87.9623031616211, -70.13690185546875,
            -59.40851974487305, -84.4076919555664, -53.747413635253906
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [-87.9623031616211],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': -87.9623031616211,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceMin float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [-87.9623031616211],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMin float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [
            -87.9623031616211, -53.747413635253906, -84.4076919555664,
            -55.97655487060547
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceMin float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceMinInput': {
          'data': [
            -58.76195526123047,  -87.9623031616211,  -70.13690185546875,
            -53.61766815185547,  -39.50931167602539, 76.48815155029297,
            -18.705087661743164, 44.78261947631836,  30.70233917236328,
            61.46361541748047,   77.84043884277344,  -53.747413635253906,
            -31.713542938232422, -9.735438346862793, 77.9365234375,
            99.01705932617188,   73.39929962158203,  92.0845947265625,
            -59.40851974487305,  -84.4076919555664,  75.88834381103516,
            96.02651977539062,   -55.97655487060547, -1.7911018133163452
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceMin',
        'arguments': [
          {'input': 'reduceMinInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceMinOutput'
      }],
      'expectedOutputs': {
        'reduceMinOutput': {
          'data': [
            -87.9623031616211, -53.747413635253906, -84.4076919555664,
            -55.97655487060547
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceProduct tests
  {
    'name': 'reduceProduct float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [-68.75911712646484],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': -68.75911712646484,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [-68.75911712646484],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments':
            [{'input': 'reduceProductInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': -68.75911712646484,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 1D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 1D tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [{'input': 'reduceProductInput'}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments':
            [{'input': 'reduceProductInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [
            7519895, -1292816.375, 2441721.75, -110637.7734375, -7380313.5,
            -818030.5
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments':
            [{'input': 'reduceProductInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [
            4227263.5, -446960.5625, 3811296.75, 1280298.5, -1343475.375,
            1280118.75
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [1.5855958784642327e+37],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': 1.5855958784642327e+37,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceProduct float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [1.5855958784642327e+37],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceProduct float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [-3638925568, 6523364352, -414643360, 1610916352],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceProduct float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceProductInput': {
          'data': [
            -68.75911712646484, 99.44961547851562,   24.86055564880371,
            -44.23515319824219, -22.699743270874023, 79.97555541992188,
            14.4650239944458,   49.23109436035156,   30.058706283569336,
            69.45106506347656,  -20.15709686279297,  -58.0255126953125,
            51.896610260009766, -2.020799160003662,  39.392974853515625,
            26.78073501586914,  -97.97651672363281,  48.66154479980469,
            -85.19523620605469, -18.16986083984375,  64.83759307861328,
            -14.95883846282959, -74.50932312011719,  -11.319679260253906
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceProduct',
        'arguments': [
          {'input': 'reduceProductInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceProductOutput'
      }],
      'expectedOutputs': {
        'reduceProductOutput': {
          'data': [-3638925568, 6523364352, -414643360, 1610916352],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceSum tests
  {
    'name': 'reduceSum float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [69.6038589477539],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 69.6038589477539,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [69.6038589477539],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 69.6038589477539,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            -51.77016830444336,  -34.46467971801758,  -40.98350524902344,
            -83.34922790527344,  -67.67525482177734,  -18.7031192779541,
            -20.28106117248535,  -20.12305450439453,  -83.63451385498047,
            -23.651084899902344, -10.208438873291016, -36.2129020690918,
            -76.26201629638672,  -9.094745635986328,  -53.889339447021484,
            -67.52340698242188,  -71.14580535888672,  -82.04484558105469,
            -96.29924774169922,  -68.46700286865234,  -26.107192993164062,
            -68.0182113647461,   -4.8330769538879395, -48.900699615478516
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': -1163.642578125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            56, 90, 67, 33, 20, 58, 22, 15, 86, 79, 59, 99,
            16, 95, 67, 11, 60, 89, 50, 57, 77, 89, 10, 2
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1307,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            -55, -36, -74, -17, -67, -95, -3,  -67, -95, -13, -45, -9,
            -33, -98, -86, -11, -70, -44, -31, -68, -79, -24, -60, -36
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': -1216,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [{'input': 'reduceSumInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [
            210.44241333007812, 104.54179382324219, 226.2177734375,
            239.025390625, 260.405029296875, 273.2470397949219
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments':
            [{'input': 'reduceSumInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [
            179.63900756835938, 260.37457275390625, 219.3611297607422,
            246.83712768554688, 157.4895782470703, 250.1780242919922
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [1313.87939453125],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'}, {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': 1313.87939453125,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSum float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'}, {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [1313.87939453125],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSum float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [
            355.21942138671875, 185.98255920410156, 362.3598937988281,
            410.3175354003906
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSum float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumInput': {
          'data': [
            69.6038589477539,  99.17485809326172,  32.78234100341797,
            8.881362915039062, 16.094295501708984, 11.80689525604248,
            32.64223861694336, 43.99836349487305,  77.01776885986328,
            79.79425811767578, 45.00794982910156,  24.397796630859375,
            57.502685546875,   57.60173034667969,  80.26985931396484,
            43.65110778808594, 87.5036849975586,   94.50203704833984,
            35.54289627075195, 42.856414794921875, 88.58631896972656,
            98.85772705078125, 25.626853942871094, 60.1761360168457
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSum',
        'arguments': [
          {'input': 'reduceSumInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceSumOutput'
      }],
      'expectedOutputs': {
        'reduceSumOutput': {
          'data': [
            355.21942138671875, 185.98255920410156, 362.3598937988281,
            410.3175354003906
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  },

  // reduceSumSquare tests
  {
    'name': 'reduceSumSquare float32 0D constant tensor default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [52.5615348815918],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 2762.71484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 0D constant tensor empty axes',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [52.5615348815918],
          'descriptor': {'dimensions': [], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments':
            [{'input': 'reduceSumSquareInput'}, {'options': {'axes': []}}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 2762.71484375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSumSquare float32 1D constant tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'},
          'constant': true
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 1D tensor all positive default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 1D tensor all negative default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            -21.45201301574707,    -57.30725860595703,  -72.8390121459961,
            -0.059761520475149155, -71.73678588867188,  -44.61909103393555,
            -43.12002182006836,    -91.3373794555664,   -33.17243957519531,
            -48.555931091308594,   -95.6286392211914,   -20.876630783081055,
            -16.690837860107422,   -39.52110290527344,  -7.5107855796813965,
            -90.59027099609375,    -42.21683120727539,  -76.74274444580078,
            -98.22420501708984,    -60.272953033447266, -74.73202514648438,
            -8.543684005737305,    -59.888736724853516, -17.99894142150879
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 80052.015625,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSumSquare float32 1D tensor all positive integers default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52, 48, 2,  66, 30, 39, 14, 23, 81, 94, 78, 64,
            38, 16, 63, 11, 46, 95, 17, 47, 40, 53, 87, 43
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 71347,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSumSquare float32 1D tensor all negative integers default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            -10, -60, -69, -88, -35, -84, -74, -42, -93, -26, -40, -55,
            -92, -26, -39, -30, -61, -16, -16, -36, -9,  -89, -45, -29
          ],
          'descriptor': {'dimensions': [24], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73634,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 2D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [4, 6], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 3D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 4D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 5D tensor default options',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 1, 4, 1, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [{'input': 'reduceSumSquareInput'}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 3D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments':
            [{'input': 'reduceSumSquareInput'}, {'options': {'axes': [2]}}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [
            9709.013671875, 10817.7685546875, 14548.470703125, 16385.8515625,
            9287.357421875, 12527.3974609375
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 4D tensor options.axes',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments':
            [{'input': 'reduceSumSquareInput'}, {'options': {'axes': [0, 2]}}],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [
            8585.87109375, 7700.654296875, 19889.1796875, 7113.0439453125,
            16775.708984375, 13211.3994140625
          ],
          'descriptor': {'dimensions': [2, 3], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 3D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 3D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 3, 4], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [73275.859375],
          'descriptor': {'dimensions': [1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 4D tensor options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'keepDimensions': false}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': 73275.859375,
          'descriptor': {'dimensions': [], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name': 'reduceSumSquare float32 4D tensor options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'keepDimensions': true}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [73275.859375],
          'descriptor': {'dimensions': [1, 1, 1, 1], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSumSquare float32 4D tensor options.axes with options.keepDimensions=false',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': false}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [
            12302.474609375, 22772.77734375, 26919.09765625, 11281.5068359375
          ],
          'descriptor': {'dimensions': [2, 2], 'dataType': 'float32'}
        }
      }
    }
  },
  {
    'name':
        'reduceSumSquare float32 4D tensor options.axes with options.keepDimensions=true',
    'graph': {
      'inputs': {
        'reduceSumSquareInput': {
          'data': [
            52.5615348815918,   2.6261062622070312, 82.04877471923828,
            14.401411056518555, 33.96051788330078,  83.9383773803711,
            47.445045471191406, 19.177289962768555, 13.493006706237793,
            44.152381896972656, 86.53118133544922,  70.20919799804688,
            25.67262840270996,  79.73770141601562,  66.42284393310547,
            70.40363311767578,  13.503327369689941, 41.225399017333984,
            6.654552936553955,  85.79743957519531,  89.91349029541016,
            53.55647277832031,  39.48537063598633,  3.9460408687591553
          ],
          'descriptor': {'dimensions': [2, 2, 2, 3], 'dataType': 'float32'}
        }
      },
      'operators': [{
        'name': 'reduceSumSquare',
        'arguments': [
          {'input': 'reduceSumSquareInput'},
          {'options': {'axes': [1, 3], 'keepDimensions': true}}
        ],
        'outputs': 'reduceSumSquareOutput'
      }],
      'expectedOutputs': {
        'reduceSumSquareOutput': {
          'data': [
            12302.474609375, 22772.77734375, 26919.09765625, 11281.5068359375
          ],
          'descriptor': {'dimensions': [2, 1, 2, 1], 'dataType': 'float32'}
        }
      }
    }
  }
];

if (navigator.ml) {
  reductionOperatorsTests.forEach((test) => {
    webnn_conformance_test(
        buildGraphAndCompute, getReductionOperatorsPrecisionTolerance, test);
  });
} else {
  test(() => assert_implements(navigator.ml, 'missing navigator.ml'));
}
