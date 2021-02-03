// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function webGpuInit() {
  const canvas = document.createElement('canvas');
  canvas.width = 1600;
  canvas.height = 900;

  if (!navigator.gpu) {
    console.warn('Webgpu not supported. navigator.gpu fails!');
    return null;
  }

  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    console.warn('Webgpu not supported. navigator.gpu.requestAdapter fails!');
    return null;
  }

  const device = await adapter.requestDevice();
  if (!device) {
    console.warn('Webgpu not supported. adapter.requestDevice() fails!');
    return null;
  }

  const context = canvas.getContext('gpupresent');
  if (!context) {
    console.warn('Webgpu not supported. canvas.getContext(gpupresent) fails!');
    return null;
  }

  return { device, canvas, context };
}

  const wgslShaders = {
    vertex: `
[[location(0)]] var<in> position : vec3<f32>;
[[location(1)]] var<in> uv : vec2<f32>;

[[location(0)]] var<out> fragUV : vec2<f32>;
[[builtin(position)]] var<out> Position : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  Position = vec4<f32>(position, 1.0);
  fragUV = uv;
  return;
}
`,

    fragment: `
[[binding(0), group(0)]] var<uniform_constant> mySampler: sampler;
[[binding(1), group(0)]] var<uniform_constant> myTexture: texture_2d<f32>;

[[location(0)]] var<in> fragUV : vec2<f32>;
[[location(0)]] var<out> outColor : vec4<f32>;

[[stage(fragment)]]
fn main() -> void {
  outColor = textureSample(myTexture, mySampler, fragUV);
  return;
}
`,
  };

async function webGpuLoadVideo(video, canvas, context, device) {
  const swapChainFormat = 'bgra8unorm';

  const rectVerts = new Float32Array([
    1.0, 1.0, 0.0, 1.0, 0.0,
    1.0, -1.0, 0.0, 1.0, 1.0,
    -1.0, -1.0, 0.0, 0.0, 1.0,
    1.0, 1.0, 0.0, 1.0, 0.0,
    -1.0, -1.0, 0.0, 0.0, 1.0,
    -1.0, 1.0, 0.0, 0.0, 0.0,
  ]);

  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });
  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  const swapChain = context.configureSwapChain({
    device,
    format: swapChainFormat,
  });

  const pipeline = device.createRenderPipeline({
    vertexStage: {
      module: device.createShaderModule({
        code: wgslShaders.vertex,
      }),
      entryPoint: 'main',
    },
    fragmentStage: {
      module: device.createShaderModule({
        code: wgslShaders.fragment,
      }),
      entryPoint: 'main',
    },
    primitiveTopology: 'triangle-list',
    vertexState: {
      vertexBuffers: [
        {
          arrayStride: 20,
          attributes: [
            {
              // position
              shaderLocation: 0,
              offset: 0,
              format: 'float3',
            },
            {
              // uv
              shaderLocation: 1,
              offset: 12,
              format: 'float2',
            },
          ],
        },
      ],
    },

    colorStates: [
      {
        format: swapChainFormat,
      },
    ],
  });

  const sampler = device.createSampler({
    magFilter: 'linear',
    minFilter: 'linear',
  });

  const videoTexture = device.createTexture({
    size: {
      width: video.videoWidth,
      height: video.videoHeight,
      depth: 1,
    },
    format: 'rgba8unorm',
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
  });

  const uniformBindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      {
        binding: 0,
        resource: sampler,
      },
      {
        binding: 1,
        resource: videoTexture.createView(),
      },
    ],
  });

  createImageBitmap(video).then((videoFrame) => {
    device.queue.copyImageBitmapToTexture(
      { imageBitmap: videoFrame, origin: { x: 0, y: 0 } },
      { texture: videoTexture },
      { width: video.videoWidth, height: video.videoHeight, depth: 1 }
    );

    const commandEncoder = device.createCommandEncoder();
    const textureView = swapChain.getCurrentTexture().createView();

    const renderPassDescriptor = {
      colorAttachments: [
        {
          attachment: textureView,
          loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    };

    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setVertexBuffer(0, verticesBuffer);
    passEncoder.setBindGroup(0, uniformBindGroup);
    passEncoder.draw(6);
    passEncoder.endPass();
    device.queue.submit([commandEncoder.finish()]);
  });

  container.appendChild(canvas);
}


