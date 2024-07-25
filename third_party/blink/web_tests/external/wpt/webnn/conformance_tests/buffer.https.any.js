// META: title=test WebNN API buffer operations
// META: global=window,dedicatedworker
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils_validation.js
// META: script=../resources/utils.js
// META: timeout=long

'use strict';

// https://www.w3.org/TR/webnn/#api-mlbuffer

const bytesPerDataType = (dataType) => {
  if (dataType === 'int8' || dataType === 'uint8') {
    return 1;
  } else if (dataType === 'float16') {
    return 2;
  } else if (
      dataType === 'float32' || dataType === 'int32' || dataType === 'uint32') {
    return 4;
  } else if (dataType === 'int64' || dataType === 'uint64') {
    return 8;
  } else {
    throw new AssertionError(`Data type '${dataType}' is not supported`);
  }
};

const sizeOfDescriptor = (descriptor) => {
  return descriptor.dimensions.reduce(
      (accumulator, currentValue) => accumulator * currentValue,
      bytesPerDataType(descriptor.dataType));
};

const getDescriptorFromBuffer = (buffer) => {
  return {dataType: buffer.dataType, dimensions: buffer.shape};
};

/**
 * Checks if MLBuffer is implemented or not.
 * @param {MLContext} mlContext - A ML context to test for MLBuffer support.
 * @returns {Boolean} True if MLBuffer is supported; otherwise, False.
 */
const isMLBufferSupported = (mlContext) => {
  return (
      createBuffer(mlContext, {dataType: 'int32', dimensions: [2, 2]}) !==
      undefined);
};

/**
 * WebNN buffer creation.
 * @param {MLContext} context - the context used to create the buffer.
 * @param {MLBufferDescriptor} bufferDescriptor - intended specs of the buffer.
 * @returns {MLBuffer} the created buffer.
 */
const createBuffer = (context, bufferDescriptor) => {
  let buffer;
  try {
    buffer = context.createBuffer(bufferDescriptor);
    assert_equals(
        buffer.dataType, bufferDescriptor.dataType,
        'buffer data types do not match');
    assert_array_equals(
        buffer.shape, bufferDescriptor.dimensions,
        'buffer shapes do not match');
  } catch (e) {
    assert_true(e instanceof DOMException);
    assert_equals(e.name, 'NotSupportedError');
  }
  return buffer;
};

/**
 * WebNN destroy buffer twice test.
 * @param {String} testName - The name of the test operation.
 */
const testDestroyWebNNBuffer = (testName) => {
  let context;
  let buffer;
  promise_setup(async () => {
    try {
      context = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
    buffer = createBuffer(context, {dataType: 'int32', dimensions: [2, 3]});
  });
  promise_test(async () => {
    // MLBuffer is not supported for this deviceType.
    if (buffer === undefined) {
      return;
    }
    buffer.destroy();
    buffer.destroy();
  }, `${testName}`);
};

/**
 * WebNN create buffer test.
 * @param {String} testName - The name of the test operation.
 * @param {MLBufferDescriptor} bufferDescriptor - The intended buffer specs.
 */
const testCreateWebNNBuffer = (testName, bufferDescriptor) => {
  let context;

  promise_setup(async () => {
    try {
      context = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
  });
  promise_test(async () => {
    createBuffer(context, bufferDescriptor);
  }, `${testName} / ${bufferDescriptor.dataType}`);
};

/**
 * Same as above, but expect creating the buffer to fail.
 * @param {String} testName - The name of the test operation.
 * @param {MLBufferDescriptor} bufferDescriptor - The intended buffer specs.
 */
const testCreateWebNNBufferFails = (testName, bufferDescriptor) => {
  let context;

  promise_setup(async () => {
    try {
      context = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
  });
  promise_test(async () => {
    assert_throws_js(TypeError, () => context.createBuffer(bufferDescriptor));
  }, `${testName} / ${bufferDescriptor.dataType}`);
};

/**
 * Asserts the buffer data in MLBuffer matches expected.
 * @param {MLContext} mlContext - The context used to create the buffer.
 * @param {MLBuffer} mlBuffer - The buffer to read and compare data.
 * @param {Array} expected - Array of the expected data in the buffer.
 */
const assert_buffer_data_equals = async (mlContext, mlBuffer, expected) => {
  const actual = await mlContext.readBuffer(mlBuffer);
  assert_array_equals(
      new expected.constructor(actual), expected,
      'Read buffer data equals expected data.');
};

/**
 * WebNN write buffer operation test.
 * @param {String} testName - The name of the test operation.
 */
const testWriteWebNNBuffer = (testName) => {
  let mlContext;
  promise_setup(async () => {
    try {
      mlContext = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
  });

  promise_test(async () => {
    const descriptor = {dataType: 'int32', dimensions: [1]};
    let mlBuffer = createBuffer(mlContext, descriptor);

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    const bufferByteLength = sizeOfDescriptor(descriptor);
    let arrayBuffer = new ArrayBuffer(bufferByteLength);

    // Writing with a size that goes past that source buffer length.
    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(arrayBuffer), /*srcOffset=*/ 0,
            /*srcSize=*/ bufferByteLength + 1));
    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(arrayBuffer), /*srcOffset=*/ 3,
            /*srcSize=*/ bufferByteLength));

    // Writing with a source offset that is out of range of the source size.
    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(arrayBuffer),
            /*srcOffset=*/ bufferByteLength + 1));

    // Writing with a source offset that is out of range of implicit copy size.
    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(arrayBuffer),
            /*srcOffset=*/ bufferByteLength + 1, /*srcSize=*/ undefined));

    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(arrayBuffer), /*srcOffset=*/ undefined,
            /*srcSize=*/ bufferByteLength + 1));

    assert_throws_js(
        TypeError,
        () => mlContext.writeBuffer(
            mlBuffer, Uint8Array.from([0xEE, 0xEE, 0xEE, 0xEE, 0xEE])));
  }, `${testName} / error`);

  promise_test(async () => {
    const descriptor = {dataType: 'int32', dimensions: [2, 2]};
    let mlBuffer = createBuffer(mlContext, descriptor);

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Writing data to a destroyed MLBuffer should throw.
    mlBuffer.destroy();

    assert_throws_dom(
        'InvalidStateError',
        () => mlContext.writeBuffer(
            mlBuffer, new Uint8Array(sizeOfDescriptor(descriptor))));
  }, `${testName} / destroy`);

  promise_test(async () => {
    const bufferDescriptor = {dataType: 'int32', dimensions: [2, 3]};
    let mlBuffer = createBuffer(mlContext, bufferDescriptor);

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    let anotherMLContext = await navigator.ml.createContext(contextOptions);
    let anotherMLBuffer = createBuffer(anotherMLContext, bufferDescriptor);

    let inputData =
        new Uint8Array(sizeOfDescriptor(bufferDescriptor)).fill(0xAA);
    assert_throws_js(
        TypeError, () => mlContext.writeBuffer(anotherMLBuffer, inputData));
    assert_throws_js(
        TypeError, () => anotherMLContext.writeBuffer(mlBuffer, inputData));
  }, `${testName} / context_mismatch`);
};

/**
 * WebNN read buffer operation test.
 * @param {String} testName - The name of the test operation.
 */
const testReadWebNNBuffer = (testName) => {
  let mlContext;
  promise_setup(async () => {
    try {
      mlContext = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
  });

  promise_test(async t => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [2, 2]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Reading a destroyed MLBuffer should reject.
    mlBuffer.destroy();

    await promise_rejects_dom(
        t, 'InvalidStateError', mlContext.readBuffer(mlBuffer));
  }, `${testName} / destroy`);

  promise_test(async () => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [1]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Initialize the buffer.
    mlContext.writeBuffer(mlBuffer, Uint8Array.from([0xAA, 0xAA, 0xAA, 0xAA]));

    mlContext.writeBuffer(mlBuffer, Uint32Array.from([0xBBBBBBBB]));
    await assert_buffer_data_equals(
        mlContext, mlBuffer, Uint32Array.from([0xBBBBBBBB]));
    ;
  }, `${testName} / full_size`);

  promise_test(async () => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [1]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Initialize the buffer.
    mlContext.writeBuffer(mlBuffer, Uint8Array.from([0xAA, 0xAA, 0xAA, 0xAA]));

    // Writing to the remainder of the buffer from source offset.
    mlContext.writeBuffer(
        mlBuffer, Uint8Array.from([0xCC, 0xCC, 0xBB, 0xBB]),
        /*srcOffset=*/ 2);
    await assert_buffer_data_equals(
        mlContext, mlBuffer, Uint8Array.from([0xBB, 0xBB, 0xAA, 0xAA]));
  }, `${testName} / src_offset_only`);

  promise_test(async () => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [1]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Initialize the buffer.
    mlContext.writeBuffer(mlBuffer, Uint8Array.from([0xAA, 0xAA, 0xAA, 0xAA]));

    // Writing with both a source offset and size.
    mlContext.writeBuffer(
        mlBuffer, Uint8Array.from([0xDD, 0xDD, 0xCC, 0xDD]),
        /*srcOffset=*/ 2, /*srcSize=*/ 1);
    await assert_buffer_data_equals(
        mlContext, mlBuffer, Uint8Array.from([0xCC, 0xAA, 0xAA, 0xAA]));
  }, `${testName} / src_offset_and_size`);

  promise_test(async () => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [1]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    // Initialize the buffer.
    mlContext.writeBuffer(mlBuffer, Uint8Array.from([0xAA, 0xAA, 0xAA, 0xAA]));

    // Using an offset allows a larger source buffer to fit.
    mlContext.writeBuffer(
        mlBuffer, Uint8Array.from([0xEE, 0xEE, 0xEE, 0xEE, 0xEE]),
        /*srcOffset=*/ 1);
    await assert_buffer_data_equals(
        mlContext, mlBuffer, Uint8Array.from([0xEE, 0xEE, 0xEE, 0xEE]));
  }, `${testName} / larger_src_data`);

  promise_test(async () => {
    let mlBuffer =
        createBuffer(mlContext, {dataType: 'int32', dimensions: [1]});

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    const inputData = [0xAA, 0xAA, 0xAA, 0xAA];

    // Writing with a source offset of undefined should be treated as 0.
    mlContext.writeBuffer(
        mlBuffer, Uint8Array.from(inputData), /*srcOffset=*/ undefined,
        /*srcSize=*/ inputData.length);
    await assert_buffer_data_equals(
        mlContext, mlBuffer, Uint8Array.from(inputData));
  }, `${testName} / no_src_offset`);

  promise_test(async t => {
    const bufferDescriptor = {dataType: 'int32', dimensions: [2, 3]};
    let mlBuffer = createBuffer(mlContext, bufferDescriptor);

    // MLBuffer was unsupported for the deviceType.
    if (mlBuffer === undefined) {
      return;
    }

    let anotherMLContext = await navigator.ml.createContext(contextOptions);
    let anotherMLBuffer = createBuffer(anotherMLContext, bufferDescriptor);

    await promise_rejects_js(
        t, TypeError, mlContext.readBuffer(anotherMLBuffer));
    await promise_rejects_js(
        t, TypeError, anotherMLContext.readBuffer(mlBuffer));
  }, `${testName} / context_mismatch`);
};

/**
 * WebNN dispatch buffer operation test.
 * @param {String} testName - The name of the test operation.
 */
const testDispatchWebNNBuffer = (testName) => {
  let mlContext;
  let mlGraph;
  const shape = [3, 5];
  let inputs = {};
  let outputs = {};
  promise_setup(async () => {
    try {
      mlContext = await navigator.ml.createContext(contextOptions);
    } catch (e) {
      throw new AssertionError(
          `Unable to create context for ${variant} variant. ${e}`);
    }
    // Construct a simple graph: A = B + C, with two outputs.
    const builder = new MLGraphBuilder(mlContext);
    const descriptor = {dataType: 'float32', dimensions: shape};
    const lhsOperand = builder.input('lhs', descriptor);
    const rhsOperand = builder.input('rhs', descriptor);
    const output1Operand = builder.add(lhsOperand, rhsOperand);
    const output2Operand = builder.add(lhsOperand, rhsOperand);
    mlGraph = await builder.build(
        {'output1': output1Operand, 'output2': output2Operand});
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }
    inputs = {
      'lhs': mlContext.createBuffer(descriptor),
      'rhs': mlContext.createBuffer(descriptor),
    };
    outputs = {
      'output1': mlContext.createBuffer(descriptor),
      'output2': mlContext.createBuffer(descriptor),
    };
  });

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    let anotherMLContext = await navigator.ml.createContext(contextOptions);

    // Control case, same context.
    mlContext.dispatch(mlGraph, inputs, outputs);

    // Test the wrong context being used for inputs.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': anotherMLContext.createBuffer(
                  getDescriptorFromBuffer(inputs['lhs'])),
              'rhs': inputs['rhs'],
            },
            outputs));

    // Test the wrong context being used for outputs.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': anotherMLContext.createBuffer(
          getDescriptorFromBuffer(outputs['output1'])),
      'output2': outputs['output2'],
    }));
  }, `${testName} / context_mismatch`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    // Control case, valid buffers.
    mlContext.dispatch(mlGraph, inputs, outputs);

    // Input is a different shape.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': mlContext.createBuffer({
                dataType: inputs['lhs'].dataType,
                // Input rank is too high.
                dimensions: inputs['lhs'].shape.concat([2])
              }),
              'rhs': inputs['rhs'],
            },
            outputs));

    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': inputs['lhs'],
              'rhs': mlContext.createBuffer({
                dataType: inputs['rhs'].dataType,
                // Input rank is too low.
                dimensions: inputs['rhs'].shape.slice(1)
              }),
            },
            outputs));

    // Output is a different shape. Dimension value is too large.
    let output1WrongShape = [...outputs['output1'].shape];
    output1WrongShape[0] += 2;
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': mlContext.createBuffer({
        dataType: outputs['output1'].dataType,
        dimensions: output1WrongShape
      }),
      'output2': outputs['output2'],
    }));

    // Output is a different shape. Dimension value is too small.
    let output2WrongShape = [...outputs['output2'].shape];
    output2WrongShape[1] -= 1;
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1'],
      'output2': mlContext.createBuffer({
        dataType: outputs['output2'].dataType,
        dimensions: output2WrongShape
      }),
    }));
  }, `${testName} / invalid shape`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    // Control case, valid buffers.
    mlContext.dispatch(mlGraph, inputs, outputs);

    // Inputs are a different data type.
    const inputWrongDataType = 'int32';
    assert_not_equals(inputs['lhs'].dataType, inputWrongDataType);
    assert_not_equals(inputs['rhs'].dataType, inputWrongDataType);
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': mlContext.createBuffer({
                dataType: inputWrongDataType,
                dimensions: inputs['lhs'].shape
              }),
              'rhs': inputs['rhs'],
            },
            outputs));

    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': inputs['lhs'],
              'rhs': mlContext.createBuffer({
                dataType: inputWrongDataType,
                dimensions: inputs['rhs'].shape
              }),
            },
            outputs));

    // Outputs are a different data type.
    const outputWrongDataType = 'int32';
    assert_not_equals(outputs['output1'].dataType, outputWrongDataType);
    assert_not_equals(outputs['output2'].dataType, outputWrongDataType);
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': mlContext.createBuffer({
        dataType: outputWrongDataType,
        dimensions: outputs['output1'].shape
      }),
      'output2': outputs['output2'],
    }));

    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1'],
      'output2': mlContext.createBuffer({
        dataType: outputWrongDataType,
        dimensions: outputs['output2'].shape
      }),
    }));
  }, `${testName} / invalid data type`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    // Control case, valid names.
    mlContext.dispatch(mlGraph, inputs, outputs);

    // No names is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, {}, {}));

    // Input name is invalid.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'aDifferentInputName': inputs['lhs'],
              'rhs': inputs['rhs'],
            },
            outputs));

    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': inputs['lhs'],
              'aDifferentInputName': inputs['rhs'],
            },
            outputs));

    // Output name is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'aDifferentOutputName': outputs['output1'],
      'output2': outputs['output2'],
    }));

    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1'],
      'aDifferentOutputName': outputs['output2'],
    }));

    // Too few named inputs is invalid.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': inputs['lhs'],
            },
            outputs));

    // Too many named inputs is invalid.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': inputs['lhs'],
              'rhs': inputs['rhs'],
              'aDifferentInputName': mlContext.createBuffer(
                  getDescriptorFromBuffer(inputs['rhs'])),
            },
            outputs));

    // Too few named outputs is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1']
    }));

    // Too many named outputs is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1'],
      'output2': outputs['output2'],
      'aDifferentOutputName':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    }));
  }, `${testName} / invalid_name`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    // Control case, valid buffers.
    mlContext.dispatch(mlGraph, inputs, outputs);

    // Same buffer used as outputs more than once is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': outputs['output1'],
      'output2': outputs['output1'],
    }));

    // Same buffer used as input and output is invalid.
    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': inputs['lhs'],
      'output2': outputs['output2'],
    }));

    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': outputs['output1'],
              'rhs': inputs['rhs'],
            },
            outputs));

    // Buffer that does not exist is invalid.
    assert_throws_js(
        TypeError,
        () => mlContext.dispatch(
            mlGraph, {
              'lhs': undefined,
              'rhs': inputs['rhs'],
            },
            outputs));

    assert_throws_js(TypeError, () => mlContext.dispatch(mlGraph, inputs, {
      'output1': undefined,
      'output2': outputs['output2'],
    }));
  }, `${testName} / invalid_buffer`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatch1Outputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    const dispatch2Outputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const inputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData);

    // Output_1 = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatch1Outputs);

    // Output_2 = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatch2Outputs);

    await assert_buffer_data_equals(
        mlContext, dispatch1Outputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));

    await assert_buffer_data_equals(
        mlContext, dispatch1Outputs['output2'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));

    await assert_buffer_data_equals(
        mlContext, dispatch2Outputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));

    await assert_buffer_data_equals(
        mlContext, dispatch2Outputs['output2'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));
  }, `${testName} / same_inputs`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatch1Inputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatch2Inputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatchOutputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const input1Data =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatch1Inputs['lhs'], input1Data);
    mlContext.writeBuffer(dispatch1Inputs['rhs'], input1Data);

    const input2Data =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(2.0);
    mlContext.writeBuffer(dispatch2Inputs['lhs'], input2Data);
    mlContext.writeBuffer(dispatch2Inputs['rhs'], input2Data);

    // Output = LHS_1 + RHS_1 = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatch1Inputs, dispatchOutputs);

    // Output = LHS_2 + RHS_2 = 2 + 2 = 4
    mlContext.dispatch(mlGraph, dispatch2Inputs, dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(4.0));

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output2'],
        new Float32Array(sizeOfShape(shape)).fill(4.0));
  }, `${testName} / same_outputs`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatchOutputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const inputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData);

    // Output = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output2'],
        new Float32Array(sizeOfShape(shape)).fill(2.0));
  }, `${testName} / same_inputs_and_outputs`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatch1Outputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    const dispatch2Outputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const inputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData);

    // Output_1 = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatch1Outputs);

    // Output_2 = Output_1_LHS + Output_1_RHS = 2 + 2 = 4
    mlContext.dispatch(
        mlGraph, {
          'lhs': dispatch1Outputs['output1'],
          'rhs': dispatch1Outputs['output2'],
        },
        dispatch2Outputs);

    // Output_1 = Output_2_LHS + Output_2_RHS = 4 + 4 = 8
    mlContext.dispatch(
        mlGraph, {
          'lhs': dispatch2Outputs['output1'],
          'rhs': dispatch2Outputs['output2'],
        },
        dispatch1Outputs);

    await assert_buffer_data_equals(
        mlContext, dispatch1Outputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(8));

    await assert_buffer_data_equals(
        mlContext, dispatch1Outputs['output2'],
        new Float32Array(sizeOfShape(shape)).fill(8));
  }, `${testName} / outputs_as_inputs`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    // Construct a simple graph: OUTPUT = LHS - RHS.
    const builder = new MLGraphBuilder(mlContext);
    const operandType = {dataType: 'float32', dimensions: shape};
    const lhsOperand = builder.input('lhs', operandType);
    const rhsOperand = builder.input('rhs', operandType);
    const graph =
        await builder.build({'output': builder.sub(lhsOperand, rhsOperand)});

    const lhsBuffer =
        mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs']));
    const rhsBuffer =
        mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs']));

    const dispatchOutputs = {
      'output':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1']))
    };

    // Initialize inputs
    mlContext.writeBuffer(
        lhsBuffer, new TypedArrayDict['float32'](sizeOfShape(shape)).fill(5.0));
    mlContext.writeBuffer(
        rhsBuffer, new TypedArrayDict['float32'](sizeOfShape(shape)).fill(3.0));

    // Output = LHS - RHS = 5 - 3 = 2
    mlContext.dispatch(
        graph, {
          'lhs': lhsBuffer,
          'rhs': rhsBuffer,
        },
        dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output'],
        new Float32Array(sizeOfShape(shape)).fill(2));

    // Output = RHS - LHS = 3 - 5 = -2
    mlContext.dispatch(
        graph, {
          'lhs': rhsBuffer,
          'rhs': lhsBuffer,
        },
        dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output'],
        new Float32Array(sizeOfShape(shape)).fill(-2));
  }, `${testName} / same name diff input buffers`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const outputBuffer1 =
        mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1']));
    const outputBuffer2 =
        mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2']));

    // Initialize inputs
    const inputData1 =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData1);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData1);

    // Output = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, {
      'output1': outputBuffer1,
      'output2': outputBuffer2,
    });

    // Output = LHS + RHS = 2 + 2 = 4
    const inputData2 =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(2.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData2);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData2);

    mlContext.dispatch(mlGraph, dispatchInputs, {
      'output1': outputBuffer1,
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    });

    // Ensure the last dispatch() did not modify the original second output
    // buffer.
    await assert_buffer_data_equals(
        mlContext, outputBuffer2, new Float32Array(sizeOfShape(shape)).fill(2));
  }, `${testName} / same name diff outputs buffers`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatchOutputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const inputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData);

    // Output = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    // Check destroyed input buffers cannot be re-used in subsequent dispatches.
    dispatchInputs['lhs'].destroy();
    dispatchInputs['lhs'] =
        mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs']));

    const newInputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(2.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], newInputData);

    // Output = LHS + RHS = 2 + 1 = 3
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(3));

    dispatchInputs['rhs'].destroy();
    dispatchInputs['rhs'] =
        mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs']));
    mlContext.writeBuffer(dispatchInputs['rhs'], newInputData);

    // Output = LHS + RHS = 2 + 2 = 4
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(4));
  }, `${testName} / same name diff inputs buffers destroy`);

  promise_test(async () => {
    // MLBuffer was unsupported for the deviceType.
    if (!isMLBufferSupported(mlContext)) {
      return;
    }

    const dispatchInputs = {
      'lhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['lhs'])),
      'rhs': mlContext.createBuffer(getDescriptorFromBuffer(inputs['rhs'])),
    };

    const dispatchOutputs = {
      'output1':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1'])),
      'output2':
          mlContext.createBuffer(getDescriptorFromBuffer(outputs['output2'])),
    };

    // Initialize inputs
    const inputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(1.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], inputData);
    mlContext.writeBuffer(dispatchInputs['rhs'], inputData);

    // Output = LHS + RHS = 1 + 1 = 2
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    // Check destroyed output buffers cannot be re-used in subsequent
    // dispatches.
    dispatchOutputs['output1'].destroy();
    dispatchOutputs['output1'] =
        mlContext.createBuffer(getDescriptorFromBuffer(outputs['output1']));

    const newInputData =
        new TypedArrayDict['float32'](sizeOfShape(shape)).fill(2.0);
    mlContext.writeBuffer(dispatchInputs['lhs'], newInputData);

    // Output = LHS + RHS = 2 + 1 = 3
    mlContext.dispatch(mlGraph, dispatchInputs, dispatchOutputs);

    await assert_buffer_data_equals(
        mlContext, dispatchOutputs['output1'],
        new Float32Array(sizeOfShape(shape)).fill(3));
  }, `${testName} / same name diff outputs buffers destroy`);
};

if (navigator.ml) {
  testCreateWebNNBuffer('create', {dataType: 'float16', dimensions: [2, 3]});
  testCreateWebNNBuffer('create', {dataType: 'float32', dimensions: [1, 5]});
  testCreateWebNNBuffer('create', {dataType: 'int32', dimensions: [4]});
  testCreateWebNNBuffer('create', {dataType: 'uint8', dimensions: [3, 2, 4]});

  testCreateWebNNBufferFails(
      'createFailsEmptyDimension', {dataType: 'int32', dimensions: [2, 0, 3]});
  testCreateWebNNBufferFails('createFailsTooLarge', {
    dataType: 'int32',
    dimensions: [kMaxUnsignedLong, kMaxUnsignedLong, kMaxUnsignedLong]
  });

  testDestroyWebNNBuffer('destroyTwice');
  testReadWebNNBuffer('read');
  testWriteWebNNBuffer('write');
  testDispatchWebNNBuffer('dispatch');
} else {
  test(() => assert_implements(navigator.ml, 'missing navigator.ml'));
}
