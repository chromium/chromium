// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const webGpuUtils = function() {
  const outputFormat = 'bgra8unorm';

  const wgslShaders = {
    vertex: `
var<private> quadPos : array<vec4<f32>, 4> = array<vec4<f32>, 4>(
    vec4<f32>(-1.0,  1.0, 0.0, 1.0),
    vec4<f32>(-1.0, -1.0, 0.0, 1.0),
    vec4<f32>( 1.0,  1.0, 0.0, 1.0),
    vec4<f32>( 1.0, -1.0, 0.0, 1.0));

var<private> quadUV : array<vec2<f32>, 4> = array<vec2<f32>, 4>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0));

struct VertexOutput {
  @builtin(position) Position : vec4<f32>,
  @location(0) fragUV : vec2<f32>,
}

@vertex
fn main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
  var output: VertexOutput;
  output.Position = quadPos[VertexIndex];
  output.fragUV = quadUV[VertexIndex];
  return output;
}
`,

    fragmentBlit: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_2d<f32>;

@fragment
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(myTexture, mySampler, fragUV);
}
`,

    fragmentClear: `
@fragment
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
`,

    fragmentImport: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_external;

@fragment
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSampleBaseClampToEdge(myTexture, mySampler, fragUV);
}
`,
  };

  return {
    init: async function(gpuCanvas, has_alpha = true) {
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

      const context = gpuCanvas.getContext('webgpu');
      if (!context) {
        console.error('getContext(webgpu) failed');
        return null;
      }

      context.configure({
        device: device,
        format: outputFormat,
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
        alphaMode: has_alpha ? "premultiplied" : "opaque",
      });

      return [device, context];
    },

    importExternalTextureTest: function(
      device, context, video) {
        const blitPipeline = device.createRenderPipeline({
          layout: 'auto',
          vertex: {
            module: device.createShaderModule({
              code: wgslShaders.vertex,
            }),
            entryPoint: 'main',
          },
          fragment: {
            module: device.createShaderModule({
              code: wgslShaders.fragmentImport,
            }),
            entryPoint: 'main',
            targets: [
              {
                format: outputFormat,
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
        const externalTextureDescriptor = { source: video };
        const externalTexture =
            device.importExternalTexture(externalTextureDescriptor);

        const bindGroup = device.createBindGroup({
          layout: blitPipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: sampler,
            },
            {
              binding: 1,
              resource: externalTexture,
            },
          ],
        });

        const renderPassDescriptor = {
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              loadOp: 'clear',
              clearValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
              storeOp: 'store',
            },
          ],
        };

        const commandEncoder = device.createCommandEncoder();
        const passEncoder =
            commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(blitPipeline);
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.draw(4, 1, 0, 0);
        passEncoder.end();

        device.queue.submit([commandEncoder.finish()]);
    },

    uploadToGPUTextureTest: function(
      device, context, canvasImageSource) {
      const blitPipeline = device.createRenderPipeline({
        layout: 'auto',
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
              format: outputFormat,
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

      let texture;

      texture = device.createTexture({
        size: [canvasImageSource.width, canvasImageSource.height],
        format: 'rgba8unorm',
        usage: GPUTextureUsage.COPY_DST |
               GPUTextureUsage.RENDER_ATTACHMENT |
               GPUTextureUsage.TEXTURE_BINDING
      });

      device.queue.copyExternalImageToTexture(
        {source: canvasImageSource, origin: [0, 0]},
        {texture},
        [canvasImageSource.width, canvasImageSource.height]
      );

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
            view: context.getCurrentTexture().createView(),
            loadOp: 'clear',
            clearValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
            storeOp: 'store',
          },
        ],
      };

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(blitPipeline);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.draw(4, 1, 0, 0);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);
    },

    fourColorsTest: function(device, context, width, height) {
      const clearPipeline = device.createRenderPipeline({
        layout: 'auto',
        vertex: {
          module: device.createShaderModule({
            code: wgslShaders.vertex,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: device.createShaderModule({
            code: wgslShaders.fragmentClear,
          }),
          entryPoint: 'main',
          targets: [{
            format: outputFormat,
            blend: {
              color: {
                srcFactor: 'constant',
                dstFactor: 'zero'
              },
              alpha: {
                srcFactor: 'constant',
                dstFactor: 'zero'
              },
            }
          }],
        },
        primitive: {
          topology: 'triangle-strip',
          stripIndexFormat: 'uint16',
        },
      });

      const renderPassDescriptor = {
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            loadOp: 'clear',
            clearValue: {r: 0.0, g: 0.0, b: 0.0, a: 1.0},
            storeOp: 'store',
          },
        ],
      };

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(clearPipeline);

      passEncoder.setBlendConstant([0.0, 1.0, 0.0, 1.0]);
      passEncoder.setScissorRect(0, 0, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([1.0, 0.0, 0.0, 1.0]);
      passEncoder.setScissorRect(0, height / 2, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([1.0, 1.0, 0.0, 1.0]);
      passEncoder.setScissorRect(width / 2, height / 2, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.setBlendConstant([0.0, 0.0, 1.0, 1.0]);
      passEncoder.setScissorRect(width / 2, 0, width / 2, height / 2);
      passEncoder.draw(4, 1, 0, 0);

      passEncoder.end();
      device.queue.submit([commandEncoder.finish()]);
    },
  };
}();
