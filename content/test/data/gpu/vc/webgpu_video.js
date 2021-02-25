// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function webGpuInit(canvasWidth, canvasHeight) {
  const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
  if (!adapter) {
    console.warn('navigator.gpu && navigator.gpu.requestAdapter fails!');
    return null;
  }

  const device = await adapter.requestDevice();
  if (!device) {
    console.warn('Webgpu not supported. adapter.requestDevice() fails!');
    return null;
  }

  const container = document.getElementById('container');
  const canvas = container.appendChild(document.createElement('canvas'));
  canvas.width = canvasWidth;
  canvas.height = canvasHeight;

  const context = canvas.getContext('gpupresent');
  if (!context) {
    console.warn('Webgpu not supported. canvas.getContext(gpupresent) fails!');
    return null;
  }

  return { device, context, canvas };
}

  const wgslShaders = {
    vertex: `
[[location(0)]] var<in> position : vec2<f32>;
[[location(1)]] var<in> uv : vec2<f32>;

[[location(0)]] var<out> fragUV : vec2<f32>;
[[builtin(position)]] var<out> Position : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  Position = vec4<f32>(position, 0.0, 1.0);
  fragUV = uv;
  return;
}
`,

    fragment: `
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
    vertex_icons: `
[[location(0)]] var<in> position : vec2<f32>;
[[builtin(position)]] var<out> Position : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  Position = vec4<f32>(position, 0.0, 1.0);
  return;
}
`,

    fragment_output_red: `
[[location(0)]] var<out> outColor : vec4<f32>;

[[stage(fragment)]]
fn main() -> void {
  outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  return;
}
`,
  };

function createVertexBuffer(device, videos, videoRows, videoColumns) {
  // Each video takes 6 vertices (2 triangles). Each vertice has 4 floats.
  // Therefore, each video needs 24 floats.
  // The small video at the corner is included in the vertex buffer.
  const rectVerts = new Float32Array(videos.length * 24);

  let w = 2.0 / videoColumns;
  let h = 2.0 / videoRows;
  for (let row = 0; row < videoRows; ++row) {
    for (let column = 0; column < videoColumns; ++column) {
      const array_index = (row * videoColumns + column) * 24;
      const x = -1.0 + w * column;
      const y = 1.0 - h * row;

      rectVerts.set([
        (x + w), y, 1.0, 0.0,
        (x + w), (y - h), 1.0, 1.0,
        x, (y - h), 0.0, 1.0,
        (x + w), y, 1.0, 0.0,
        x, (y - h), 0.0, 1.0,
        x, y, 0.0, 0.0,
      ], array_index);
    }
  }

  // For the small video at the corner, the last one in |videos|.
  w = w / videos[0].width * videos[videos.length - 1].width;
  h = h / videos[0].height * videos[videos.length - 1].height;
  const x = 1.0 - w;
  const y = 1.0;
  const array_index = (videos.length - 1) * 24;

  rectVerts.set([
    (x + w), y, 1.0, 0.0,
    (x + w), (y - h), 1.0, 1.0,
    x, (y - h), 0.0, 1.0,
    (x + w), y, 1.0, 0.0,
    x, (y - h), 0.0, 1.0,
    x, y, 0.0, 0.0,
  ], array_index);


  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });

  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  return verticesBuffer;
}

function createVertexBufferForIcons(device, videos, videoRows, videoColumns) {
  // Each video takes 6 vertices (2 triangles). Each vertice has 2 floats.
  // Therefore, each video needs 12 floats.
  const rectVerts = new Float32Array(videos.length * 12);

  let w = 2.0 / videoColumns;
  let h = 2.0 / videoRows;
  let wIcon = w / 10.0;
  let hIcon = h / 10.0;

  for (let row = 0; row < videoRows; ++row) {
    for (let column = 0; column < videoColumns; ++column) {
      const array_index = (row * videoColumns + column) * 12;
      const x = -1.0 + w * column;
      const y = 1.0 - h * row;
      const xIcon = x + wIcon;
      const yIcon = y - h + hIcon * 2;

      rectVerts.set([
        (xIcon + wIcon), yIcon,
        (xIcon + wIcon), (yIcon - hIcon),
        xIcon, (yIcon - hIcon),
        (xIcon + wIcon), yIcon,
        xIcon, (yIcon - hIcon),
        xIcon, yIcon,
      ], array_index);
    }
  }

  // For the small video at the corner, the last one in |videos|.
  w = w / videos[0].width * videos[videos.length - 1].width;
  h = h / videos[0].height * videos[videos.length - 1].height;
  wIcon = w / 10.0;
  hIcon = h / 10.0;

  const x = 1.0 - w;
  const y = 1.0;
  const xIcon = x + wIcon;
  const yIcon = y - h + hIcon * 2;

  array_index = (videos.length - 1) * 12;
  rectVerts.set([
    (xIcon + wIcon), yIcon,
    (xIcon + wIcon), (yIcon - hIcon),
    xIcon, (yIcon - hIcon),
    (xIcon + wIcon), yIcon,
    xIcon, (yIcon - hIcon),
    xIcon, yIcon,
  ], array_index);

  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });

  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  return verticesBuffer;
}

function webGpuDrawVideoFrames(gpuSetting, videos, videoRows, videoColumns,
                               addUI, useImportTextureApi) {
  const { device, context, canvas } = gpuSetting;

  const verticesBuffer = createVertexBuffer(device, videos, videoRows,
                         videoColumns);

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
          arrayStride: 16,
          attributes: [
            {
              // position
              shaderLocation: 0,
              offset: 0,
              format: 'float2',
            },
            {
              // uv
              shaderLocation: 1,
              offset: 8,
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

  const renderPassDescriptor = {
    colorAttachments: [
      {
        attachment: undefined, // Assigned later
        loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
      },
    ],
  };

  const sampler = device.createSampler({
    magFilter: 'linear',
    minFilter: 'linear',
  });

  const videoTextures = [];
  for (let i = 0; i < videos.length; ++i) {
    videoTextures[i] = device.createTexture({
      size: {
        width: videos[i].videoWidth,
        height: videos[i].videoHeight,
        depthOrArrayLayers: 1,
      },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
    });
  }

  const bindGroups = [];
  for (let i = 0; i < videos.length; ++i) {
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

  // For rendering the icons
  const verticesBufferForIcons =
    createVertexBufferForIcons(device, videos, videoRows, videoColumns);

  const pipelineForIcons = device.createRenderPipeline({
    vertexStage: {
      module: device.createShaderModule({
        code: wgslShaders.vertex_icons,
      }),
      entryPoint: 'main',
    },
    fragmentStage: {
      module: device.createShaderModule({
        code: wgslShaders.fragment_output_red,
      }),
      entryPoint: 'main',
    },
    primitiveTopology: 'triangle-list',
    vertexState: {
      vertexBuffers: [
        {
          arrayStride: 8,
          attributes: [
            {
              // position
              shaderLocation: 0,
              offset: 0,
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

  // The frameId is increased by one every time oneFrame() is called.
  var frameId = 0;

  // The videos are displayed at different frame rate with lower indices in
  // videos updated faster than hight indices.
  // For videos #0-#3, they are to be displayed at 30 fps.
  // For videos #3-#15, they are to be displayed at 15 fps.
  // For videos #16+, they are to be displayed at 7.5 fps.
  // Since oneFrame() is called at 30 fps, the video textures #0-#3 are copied
  // every frame, #4 -#15 are copied every other frame and #16+ are copied
  // every 4 frames.
  function GetNumOfVideosToCopyForCurrentFrame(frameId) {
    switch (frameId % 4) {
      case 0:
        return videos.length;
      case 1:
      case 3:
        return Math.min(4, videos.length);
      case 2:
        return Math.min(16, videos.length);
      default:
        console.error('Something wrong with frameId % 4');
    }
  }

  const oneFrame = () => {
    const swapChainTexture = swapChain.getCurrentTexture();
    renderPassDescriptor.colorAttachments[0].attachment = swapChainTexture
      .createView();

    const commandEncoder = device.createCommandEncoder();

    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setVertexBuffer(0, verticesBuffer);

    // These videos are displayed at different fps. Not every video needs to be
    // updated in this frame. The videos at lower indices are updated faster
    // (higher fps) than videos at higher indices. See comments of
    // GetNumOfVideosToCopyForCurrentFrame() for how the videos with different
    // fps are arranged.
    const numVideosToCopy = GetNumOfVideosToCopyForCurrentFrame(frameId);

    Promise.all(videos.slice(0, numVideosToCopy).
      map(video => createImageBitmap(video))).then((videoFrames) => {
        for (let i = 0; i < numVideosToCopy; ++i) {
          device.queue.copyImageBitmapToTexture(
            { imageBitmap: videoFrames[i], origin: { x: 0, y: 0 } },
            { texture: videoTextures[i] },
            {
              width: videos[i].videoWidth, height: videos[i].videoHeight,
              depthOrArrayLayers: 1
            }
          );
        }

        for (let i = 0; i < videos.length; ++i) {
          const firstVertex = i * 6;
          passEncoder.setBindGroup(0, bindGroups[i]);
          passEncoder.draw(6, 1, firstVertex, 0);
        }

        // Add UI on Top of all videos.
        if (addUI) {
          passEncoder.setPipeline(pipelineForIcons);
          passEncoder.setVertexBuffer(0, verticesBufferForIcons);
          passEncoder.draw(videos.length * 6);
        }
        passEncoder.endPass();
        device.queue.submit([commandEncoder.finish()]);

        frameId++;
      });
  }

  const oneFrameWithImportTextureApi = () => {
    const swapChainTexture = swapChain.getCurrentTexture();
    renderPassDescriptor.colorAttachments[0].attachment = swapChainTexture
      .createView();

    // These videos are displayed at different fps. Not every video needs to be
    // updated in this frame. The videos at lower indices are updated faster
    // than videos at higher indices. See GetNumOfVideosToCopyForCurrentFrame()
    // for how the videos with different fps are arranged.
    const numVideosToCopy = GetNumOfVideosToCopyForCurrentFrame(frameId);

    for (let i = 0; i < numVideosToCopy; ++i) {
      // Destroy the textures after submit to promptly recycle resources.
      // The textures not being imported here this time are not to be destroyed.
      // We still need to render those textures for this frame.
      videoTextures[i].destroy();
      videoTextures[i] = device.experimentalImportTexture(
        videos[i], GPUTextureUsage.SAMPLED);
    }

    const commandEncoder = device.createCommandEncoder();
    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setVertexBuffer(0, verticesBuffer);

    for (let i = 0; i < videos.length; ++i) {
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
      const firstVertex = i * 6;
      passEncoder.setBindGroup(0, bindGroups[i]);
      passEncoder.draw(6, 1, firstVertex, 0);
    }

    // Add UI on Top of all videos.
    if (addUI) {
      passEncoder.setPipeline(pipelineForIcons);
      passEncoder.setVertexBuffer(0, verticesBufferForIcons);
      passEncoder.draw(videos.length * 6);
    }
    passEncoder.endPass();
    device.queue.submit([commandEncoder.finish()]);

    frameId++;
  }

  // Call oneFrame() every 33 milliseconds to simulate 30 fps.
  if (useImportTextureApi) {
    setInterval(oneFrameWithImportTextureApi, 33);
  } else {
    setInterval(oneFrame, 33);
  }
}
