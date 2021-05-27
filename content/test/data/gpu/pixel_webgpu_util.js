// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const webGpuUtils = function() {
  const swapChainFormat = 'bgra8unorm';

  const wgslShaders = {
    vertex: `
[[builtin(vertex_index)]] var<in> VertexIndex : u32;
[[builtin(position)]] var<out> Position : vec4<f32>;

[[location(0)]] var<out> fragUV : vec2<f32>;

const quadPos : array<vec4<f32>, 4> = array<vec4<f32>, 4>(
    vec4<f32>(-1.0,  1.0, 0.0, 1.0),
    vec4<f32>(-1.0, -1.0, 0.0, 1.0),
    vec4<f32>( 1.0,  1.0, 0.0, 1.0),
    vec4<f32>( 1.0, -1.0, 0.0, 1.0));

const quadUV : array<vec2<f32>, 4> = array<vec2<f32>, 4>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0));

[[stage(vertex)]]
fn main() -> void {
  Position = quadPos[VertexIndex];
  fragUV = quadUV[VertexIndex];
  return;
}
`,

    fragmentBlit: `
[[binding(0), group(0)]] var mySampler: sampler;
[[binding(1), group(0)]] var myTexture: texture_2d<f32>;

[[location(0)]] var<in> fragUV : vec2<f32>;
[[location(0)]] var<out> outColor : vec4<f32>;

[[stage(fragment)]]
fn main() -> void {
  outColor = textureSample(myTexture, mySampler, fragUV);
  return;
}
`,
  };

  return {
    init: async function(gpuCanvas) {
      const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
      if (!adapter) {
        console.error('navigator.gpu && navigator.gpu.requestAdapter failed');
        return null;
      }

      const device = await adapter.requestDevice();
      if (!device) {
        console.error('adapter.requestDevice() failed');
        return null;
      }

      const gpuContext = gpuCanvas.getContext('gpupresent');
      if (!gpuContext) {
        console.error('getContext(gpupresent) failed');
        return null;
      }

      const swapChain = gpuContext.configureSwapChain({
        device: device,
        format: swapChainFormat,
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
      });

      return [device, swapChain];
    },

    importCanvasTest: function(device, swapChain, sourceCanvas) {
      const blitPipeline = device.createRenderPipeline({
        vertex: {
          module: device.createShaderModule({
            code: wgslShaders.vertex,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: device.createShaderModule({
            code: wgslShaders.fragmentBlit,
          }),
          entryPoint: 'main',
          targets: [
            {
              format: swapChainFormat,
            },
          ],
        },
        primitive: {
          topology: 'triangle-strip',
          stripIndexFormat: 'uint16',
        },
      });

      const sampler = device.createSampler({
        magFilter: 'linear',
        minFilter: 'linear',
      });

      const texture = device.experimentalImportTexture(
          sourceCanvas, GPUTextureUsage.SAMPLED);

      const bindGroup = device.createBindGroup({
        layout: blitPipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: sampler,
          },
          {
            binding: 1,
            resource: texture.createView(),
          },
        ],
      });

      const renderPassDescriptor = {
        colorAttachments: [
          {
            view: swapChain.getCurrentTexture().createView(),
            loadValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
          },
        ],
      };

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(blitPipeline);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.draw(4, 1, 0, 0);
      passEncoder.endPass();

      device.queue.submit([commandEncoder.finish()]);
    },
  };
}();
