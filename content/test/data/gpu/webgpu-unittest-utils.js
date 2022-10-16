// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test "enum" allows for specifying tests in webGpuUnitTests below.
const WebGpuUnitTestId = {
  RenderTest: 'render-test',
  RenderTestAsync: 'render-test-async',
  ComputeTest: 'compute-test',
  ComputeTestAsync: 'compute-test-async',
};

// Implements a set of simple standalone unit tests to test WebGPU without
// depending on the canvas. Each test returns a pair consisting of a bool
// indicating whether the test passed (true) or failed (false), and a
// potentially empty array of messages detailing why the test may have
// failed.
export const webGpuUnitTests = function() {
  //////////////////////////////////////////////////////////////////////////////
  // Private internal helpers

  // Initializes the adapter and devices for webgpu usage.
  const init = async function() {
    const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
    if (!adapter) {
      console.error('navigator.gpu && navigator.gpu.requestAdapter failed');
      return [
        null,
        null,
        ['WebGPU was unavailable and/or requesting adapter failed.']
      ];
    }
    const device = await adapter.requestDevice();
    if (!device) {
      console.error('adapter.requestDevice() failed');
      return [
        adapter,
        null,
        ['Failed to request a WebGPU device.']
      ];
    }
    return [adapter, device];
  };

  // Compares an actual array (a) to an expected one (e), returning [true, []]
  // iff the type and contents of the arrays are equal, otherwise returning
  // [false, [description]].
  const compareArrays = function(e, a) {
    if (e.constructor !== a.constructor) {
      return [
        false,
        [`Expected type '${e.constructor.name}', got '${a.constructor.name}'.`]
      ];
    }
    if (e.length !== a.length) {
      return [
        false,
        [`Expected length ${e.length}, got ${a.length}.`]
      ];
    }
    var equal = true;
    for (var i = 0; i !== e.length; i++) {
      if (e[i] != a[i]) {
        success = equal;
      }
    }
    return equal ?
        [true, []] :
        [false, [`Expected [${e.toString()}], got [${a.toString()}].`]];
  }

  // Render test base which allows for specifying whether to use async pipeline
  // creation. Renders a single pixel texture, copies it to a buffer, and
  // verifies.
  const renderTestBase = async function(useAsync) {
    const [adapter, device, errors] = await init();
    if (!adapter || !device) {
      return [false, errors];
    }

    // Create the WebGPU primitives and execute the rendering and buffer copy.
    const buffer = device.createBuffer({
      size: 4,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });
    const texture = device.createTexture({
      format: 'rgba8unorm',
      size: { width: 1, height: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });
    const view = texture.createView();
    const pipelineDesc = {
      layout: 'auto',
      vertex: {
        module: device.createShaderModule({
          code: `
            @vertex fn main(
              @builtin(vertex_index) VertexIndex : u32
              ) -> @builtin(position) vec4<f32> {
                var pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                    vec2<f32>(-1.0, -3.0),
                    vec2<f32>(3.0, 1.0),
                    vec2<f32>(-1.0, 1.0));
                return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
              }
              `,
        }),
        entryPoint: 'main',
      },
      fragment: {
        module: device.createShaderModule({
          code: `
              @fragment fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
              }
              `,
        }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },
      primitive: { topology: 'triangle-list' },
    };
    const pipeline = useAsync
          ? await device.createRenderPipelineAsync(pipelineDesc)
          : device.createRenderPipeline(pipelineDesc);
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view,
          storeOp: 'store',
          clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
        },
      ],
    });
    pass.setPipeline(pipeline);
    pass.draw(3);
    pass.end();
    encoder.copyTextureToBuffer(
        { texture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
        { buffer, bytesPerRow: 256 },
        { width: 1, height: 1, depthOrArrayLayers: 1 }
    );
    device.queue.submit([encoder.finish()]);

    // Verify the contents of the buffer that the texture was copied into.
    var success = true;
    const expected = new Uint8Array([0x00, 0xff, 0x00, 0xff]);
    await buffer.mapAsync(GPUMapMode.READ);
    const actual = new Uint8Array(buffer.getMappedRange());
    return compareArrays(expected, actual);
  };

  // Compute test base which allows for specifying whether to use async pipeline
  // creation. Fills a buffer with global_invocation_id.x and verifies the
  // contents of the buffer.
  const computeTestBase = async function(useAsync) {
    const [adapter, device, errors] = await init();
    if (!adapter || !device) {
      return [false, errors];
    }

    // Test constants.
    const n = 16;
    const size = n * 4;

    // Create the WebGPU primitives and execute the compute and buffer copy.
    const pipelineDesc = {
      layout: 'auto',
      compute: {
        module: device.createShaderModule({
          code: `
            @group(0) @binding(0) var<storage, read_write> buffer: array<u32>;

            @compute @workgroup_size(1u) fn main(
              @builtin(global_invocation_id) id: vec3<u32>
            ) {
              buffer[id.x] = id.x;
            }
            `,
        }),
        entryPoint: 'main',
      },
    };
    const pipeline = useAsync
          ? await device.createComputePipelineAsync(pipelineDesc)
          : device.createComputePipeline(pipelineDesc);
    const buffer = device.createBuffer({
      size,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });
    const result = device.createBuffer({
      size,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer } }],
    });
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatchWorkgroups(n);
    pass.end();
    encoder.copyBufferToBuffer(buffer, 0, result, 0, size);
    device.queue.submit([encoder.finish()]);

    // Verify the contents of the buffer that was copied into.
    var success = true;
    const expected = new Uint32Array([...Array(n).keys()]);
    await result.mapAsync(GPUMapMode.READ);
    const actual = new Uint32Array(result.getMappedRange());
    return compareArrays(expected, actual);
  };

  return {
    ////////////////////////////////////////////////////////////////////////////
    // Actual unit tests

    renderTest: async function() {
      return await renderTestBase(false);
    },
    renderTestAsync: async function() {
      return await renderTestBase(true);
    },
    computeTest: async function() {
      return await computeTestBase(false);
    },
    computeTestAsync: async function() {
      return await computeTestBase(true);
    },

    ////////////////////////////////////////////////////////////////////////////
    // Test driver
    runTest: async function(testId) {
      // Test running wrapper to prefix error messages with test name.
      const wrapper = async function(testId, testFunc) {
        const [success, errors] = await testFunc();
        if (success) {
          return [true, []];
        }
        return [
          false,
          [`WebGPU test '${testId}' failed with the following errors:`] +
              errors.map(function(e) { return '    ' + e; })];
      };

      switch (testId) {
        case WebGpuUnitTestId.RenderTest:
          return await wrapper(testId, this.renderTest);
          break;
        case WebGpuUnitTestId.RenderTestAsync:
          return await wrapper(testId, this.renderTestAsync);
          break;
        case WebGpuUnitTestId.ComputeTest:
          return await wrapper(testId, this.computeTest);
          break;
        case WebGpuUnitTestId.ComputeTestAsync:
          return await wrapper(testId, this.computeTestAsync);
          break;
        default:
          // Just fail for any undefined tests.
          return [false, [`Undefined WebGPU test '${testId}' specified.`]];
      }
    },
  };
}();
