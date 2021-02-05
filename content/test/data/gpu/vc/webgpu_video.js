// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function webGpuInit() {
  const canvas = document.createElement('canvas');

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

function createVertexBuffer(device, videoRows, videoColumns) {
  // Each video takes 6 vertices (2 triangles). Each vertice has 5 floats.
  // Therefore, each video needs 30 floats.
  const rectVerts = new Float32Array(videoRows * videoColumns * 30);

  for (let row = 0; row < videoRows; row++) {
    for (let column = 0; column < videoColumns; column++) {
      const index = (row * videoColumns + column) * 30;
      const w = 2.0 / videoColumns;
      const h = 2.0 / videoRows;
      const x = -1.0 + w * column;
      const y = 1.0 - h * row;

      rectVerts.set([
        (x + w), y, 0.0, 1.0, 0.0,
        (x + w), (y - h), 0.0, 1.0, 1.0,
        x, (y - h), 0.0, 0.0, 1.0,
        (x + w), y, 0.0, 1.0, 0.0,
        x, (y - h), 0.0, 0.0, 1.0,
        x, y, 0.0, 0.0, 0.0,
      ], index);
    }
  }

  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });

  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  return verticesBuffer;
}

function webGpuDrawVideoFrames(gpuSetting, videos, videoRows, videoColumns) {
  const { canvas, context, device } = gpuSetting;
  const container = document.getElementById('container');
  canvas.width = videos[0].width * videoColumns;
  canvas.height = videos[0].height * videoRows;
  container.appendChild(canvas);

  const verticesBuffer = createVertexBuffer(device, videoRows, videoColumns);

  const swapChainFormat = 'bgra8unorm';
  const swapChain = context.configureSwapChain({
    device,
    format: swapChainFormat,
    usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
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

  const videoTextures = [];
  for (let i = 0; i < videoRows * videoColumns; ++i) {
    videoTextures[i] = device.createTexture({
      size: {
        width: videos[i].videoWidth,
        height: videos[i].videoHeight,
        depth: 1,
      },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
    });
  }

  const bindGroups = [];
  for (let i = 0; i < videoRows * videoColumns; ++i) {
    bindGroups[i] = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: sampler,
        },
        {
          binding: 1,
          resource: videoTextures[i].createView(),
        },
      ],
    });
  }

  const drawOneFrame = () => {
    const textureView = swapChain.getCurrentTexture().createView();
    const renderPassDescriptor = {
      colorAttachments: [
        {
          attachment: textureView,
          loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    };

    const commandEncoder = device.createCommandEncoder();
    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setVertexBuffer(0, verticesBuffer);

    Promise.all(videos.map(video => createImageBitmap(video))).
      then((videoFrames) => {
      for (let i = 0; i < videoRows * videoColumns; ++i) {
        const firstVertex = i * 6;
        device.queue.copyImageBitmapToTexture(
          { imageBitmap: videoFrames[i], origin: { x: 0, y: 0 } },
          { texture: videoTextures[i] },
          { width: videos[i].videoWidth, height: videos[i].videoHeight, depth: 1 }
        );

        passEncoder.setBindGroup(0, bindGroups[i]);
        passEncoder.draw(6, 1, firstVertex, 0);
      }
      passEncoder.endPass();
      device.queue.submit([commandEncoder.finish()]);

      window.requestAnimationFrame(drawOneFrame);
    });
  }
  window.requestAnimationFrame(drawOneFrame);
}




