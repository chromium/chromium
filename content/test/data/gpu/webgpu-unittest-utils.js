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
// depending on the canvas.
export const webGpuUnitTests = function() {
  //////////////////////////////////////////////////////////////////////////////
  // Private internal helpers

  // Initializes the adapter and devices for webgpu usage.
  const init = async function() {
    const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
    if (!adapter) {
      console.error('navigator.gpu && navigator.gpu.requestAdapter failed');
      return [null, null];
    }
    const device = await adapter.requestDevice();
    if (!device) {
      console.error('adapter.requestDevice() failed');
      return [adapter, null];
    }
    return [adapter, device];
  };

  // Render test base which allows for specifying whether to use async pipeline
  // creation. Renders a single pixel texture, copies it to a buffer, and
  // verifies.
  const renderTestBase = async function(useAsync) {
    const [adapter, device] = await init();
    if (!adapter || !device) {
      return false;
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
    const expected = new Uint8Array([0x00, 0xff, 0x00, 0xff]);
    await buffer.mapAsync(GPUMapMode.READ);
    const actual = new Uint8Array(buffer.getMappedRange());
    if (expected.length !== actual.length) {
      return false;
    }
    for (var i = 0; i !== expected.length; i++) {
      if (expected[i] != actual[i]) {
        return false;
      }
    }
    return true;
  };

  // Compute test base which allows for specifying whether to use async pipeline
  // creation. Fills a buffer with global_invocation_id.x and verifies the
  // contents of the buffer.
  const computeTestBase = async function(useAsync) {
    const [adapter, device] = await init();
    if (!adapter || !device) {
      return false;
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
    const expected = new Uint32Array([...Array(n).keys()]);
    await result.mapAsync(GPUMapMode.READ);
    const actual = new Uint32Array(result.getMappedRange());
    if (expected.length !== actual.length) {
      return false;
    }
    for (var i = 0; i !== expected.length; i++) {
      if (expected[i] != actual[i]) {
        return false;
      }
    }
    return true;
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
      switch (testId) {
        case WebGpuUnitTestId.RenderTest:
          return await this.renderTest();
          break;
        case WebGpuUnitTestId.RenderTestAsync:
          return await this.renderTestAsync();
          break;
        case WebGpuUnitTestId.ComputeTest:
          return await this.computeTest();
          break;
        case WebGpuUnitTestId.ComputeTestAsync:
          return await this.computeTestAsync();
          break;
        default:
          // Just fail for any undefined tests.
          return false;
      }
    },
  };
}();
