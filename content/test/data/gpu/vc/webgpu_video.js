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

  const context = canvas.getContext('webgpu');
  if (!context) {
    console.warn('Webgpu not supported. canvas.getContext("webgpu") fails!');
    return null;
  }

  return {adapter, device, context, canvas};
}

const wgslShaders = {
  vertex: `
struct VertexOutput {
  @builtin(position) Position : vec4<f32>;
  @location(0) fragUV : vec2<f32>;
};

@stage(vertex) fn main(
  @location(0) position : vec2<f32>,
  @location(1) uv : vec2<f32>
) -> VertexOutput {
  var output : VertexOutput;
  output.Position = vec4<f32>(position, 0.0, 1.0);
  output.fragUV = uv;
  return output;
}
`,

  fragment_external_texture: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_external;

@stage(fragment)
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSampleLevel(myTexture, mySampler, fragUV);
}
`,

  fragment: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_2d<f32>;

@stage(fragment)
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(myTexture, mySampler, fragUV);
}
`,

  vertex_icons: `
@stage(vertex)
fn main(@location(0) position : vec2<f32>)
    -> @builtin(position) vec4<f32> {
  return vec4<f32>(position, 0.0, 1.0);
}
`,

  fragment_output_blue: `
@stage(fragment)
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(0.11328125, 0.4296875, 0.84375, 1.0);
}
`,
  fragment_output_light_blue: `
@stage(fragment)
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(0.3515625, 0.50390625, 0.75390625, 1.0);
}
`,

  fragment_output_white: `
@stage(fragment)
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
`,
};

function createVertexBuffer(device, videos, videoRows, videoColumns) {
  // Each video takes 6 vertices (2 triangles). Each vertice has 4 floats.
  // Therefore, each video needs 24 floats.
  // The small video at the corner is included in the vertex buffer.
  const rectVerts = new Float32Array(videos.length * 24);

  // Width and height of the video.
  const maxColRow = Math.max(videoColumns, videoRows);
  let w = 2.0 / maxColRow;
  let h = 2.0 / maxColRow;
  for (let row = 0; row < videoRows; ++row) {
    for (let column = 0; column < videoColumns; ++column) {
      const array_index = (row * videoColumns + column) * 24;
      // X and y of the video.
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
  const y = 1.0 - (2.0 / maxColRow) + h;
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
  // Each icon takes 6 vertices (2 triangles). Each vertice has 2 floats.
  // Therefore, each video needs 12 floats.
  const rectVerts = new Float32Array(videos.length * 12);

  // Width and height of the video.
  const maxColRow = Math.max(videoColumns, videoRows);
  let w = 2.0 / maxColRow;
  let h = 2.0 / maxColRow;
  // Width and height of the icon.
  let wIcon = w / videos[0].width * videos[0].height / 8.0;
  let hIcon = h / 8.0;

  for (let row = 0; row < videoRows; ++row) {
    for (let column = 0; column < videoColumns; ++column) {
      const array_index = (row * videoColumns + column) * 12;
      // X and y of the video.
      const x = -1.0 + w * column;
      const y = 1.0 - h * row;
      // X and y of the icon.
      const xIcon = x + w - wIcon * 2;
      const yIcon = y - hIcon;

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

  // For the icon of the small video at the corner, the last one in |videos|.
  // Width and height of the small video.
  w = w / videos[0].width * videos[videos.length - 1].width;
  h = h / videos[0].height * videos[videos.length - 1].height;
  // Width and height of the small icon.
  wIcon = w / videos[0].width * videos[0].height / 8.0;
  hIcon = h / 8.0;

  // X and y of the small video.
  const x = 1.0 - w;
  const y = 1.0 - (2.0 / maxColRow) + h;
  // X and y of the small icon.
  const xIcon = x + w - wIcon * 2;
  const yIcon = y - hIcon;

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

function createVertexBufferForAnimation(
            device, videos, videoRows, videoColumns) {
  // Create voice bar animation and borders of the last video.
  // (1) Generate 10 differnt lengths of voice bar. Each bar takes 2 triangles,
  // which are 6 vertices.
  // (2) Generate borders, consisting of 4 lines. Each line takes 2 vertices.
  // Each vertice has 2 floats.
  // Total are 10*6*2 + 4*2*2  = 120 + 16 = 136 floats.
  const rectVerts = new Float32Array(136);

  // (1) Voice bars.
  // X, Y, width and height of the first video.
  const maxColRow = Math.max(videoColumns, videoRows);
  const w = 2.0 / maxColRow;
  const h = 2.0 / maxColRow;
  const x = -1.0;
  const y = 1.0;

  // Width and height of the icon.
  const wIcon = w / videos[0].width * videos[0].height / 8.0;
  const hIcon = h / 8.0;

  // X, Y, width and height of the animated bar.
  const wPixel = w / videos[0].width;
  const wBar = wPixel * 5;
  const xBar = x + w - wIcon * 1.5 - (wPixel * 2);
  const delta = (hIcon - (wPixel * 4)) / 10;

  // 10 different length for animation.
  const bar_count = 10;
  for (let i = 0; i < bar_count; ++i) {
    const array_index = i * 12;
    const hBar = (i+1) * delta;
    const yBar = y - hIcon * 2 + hBar;

    rectVerts.set([
      (xBar + wBar), yBar,
      (xBar + wBar), (yBar - hBar),
      xBar, (yBar - hBar),
      (xBar + wBar), yBar,
      xBar, (yBar - hBar),
      xBar, yBar,
    ], array_index);
  }

  // (2) Borders of the first video
  const array_index = 10 * 12;
  rectVerts.set([
    x, y, (x + w), y,
    (x + w), y, (x + w), (y - h),
    (x + w), (y - h), x, (y - h),
    x, (y - h), x, y,
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
  const {adapter, device, context, canvas} = gpuSetting;

  const verticesBuffer = createVertexBuffer(device, videos, videoRows,
                         videoColumns);

  const swapChainFormat = context.getPreferredFormat(adapter);

  const swapChain = context.configure({
    device,
    format: swapChainFormat,
    usage: GPUTextureUsage.RENDER_ATTACHMENT,
  });

  let fragmentShaderModeul;
  if (useImportTextureApi) {
    fragmentShaderModule = device.createShaderModule({
      code: wgslShaders.fragment_external_texture,
    });
  } else {
    fragmentShaderModule = device.createShaderModule({
      code: wgslShaders.fragment,
    });
  }


  const pipeline = device.createRenderPipeline({
    vertex: {
      module: device.createShaderModule({
        code: wgslShaders.vertex,
      }),
      entryPoint: 'main',
      buffers: [{
        arrayStride: 16,
        attributes: [
          {
            // position
            shaderLocation: 0,
            offset: 0,
            format: 'float32x2',
          },
          {
            // uv
            shaderLocation: 1,
            offset: 8,
            format: 'float32x2',
          }
        ],
      }],
    },
    fragment: {
      module: fragmentShaderModule,
      entryPoint: 'main',
      targets: [{
        format: swapChainFormat,
      }]
    },
    primitive: {
      topology: 'triangle-list',
    },
  });

  const renderPassDescriptor = {
    colorAttachments: [
      {
        view: undefined, // Assigned later
        loadValue: { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
        storeOp: 'store',
      },
    ],
  };

  const sampler = device.createSampler({
    magFilter: 'linear',
    minFilter: 'linear',
  });

  const videoTextures = [];
  const bindGroups = [];

  if (!useImportTextureApi) {
    for (let i = 0; i < videos.length; ++i) {
      videoTextures[i] = device.createTexture({
        size: {
          width: videos[i].videoWidth,
          height: videos[i].videoHeight,
          depthOrArrayLayers: 1,
        },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING |
            GPUTextureUsage.RENDER_ATTACHMENT,
      });

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
  }

  const externalTextureDescriptor = [];
  for (let i = 0; i < videos.length; ++i) {
    externalTextureDescriptor[i] = {source: videos[i]};
  }

  // For rendering the icons
  const verticesBufferForIcons =
    createVertexBufferForIcons(device, videos, videoRows, videoColumns);

  const renderPipelineDescriptorForIcon = {
    vertex: {
      module: device.createShaderModule({
        code: wgslShaders.vertex_icons,
      }),
      entryPoint: 'main',
      buffers: [{
        arrayStride: 8,
        attributes: [{
          // position
          shaderLocation: 0,
          offset: 0,
          format: 'float32x2',
        }],
      }],
    },
    fragment: {
      entryPoint: 'main',
      targets: [{
        format: swapChainFormat,
      }]
    },
    primitive: {
      topology: 'triangle-list',
    }
  };

  renderPipelineDescriptorForIcon.fragment.module = device.createShaderModule({
    code: wgslShaders.fragment_output_blue,
  });
  const pipelineForIcons =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);

  // For rendering the voice bar animation
  const vertexBufferForAnimation =
          createVertexBufferForAnimation(
            device, videos, videoRows, videoColumns);

  renderPipelineDescriptorForIcon.fragment.module = device.createShaderModule({
    code: wgslShaders.fragment_output_white,
  });
  const pipelineForAnimation =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);

  // For rendering the borders of the last video
  renderPipelineDescriptorForIcon.fragment.module = device.createShaderModule({
    code: wgslShaders.fragment_output_light_blue,
  });
  renderPipelineDescriptorForIcon.primitive.topology = 'line-list';
  const pipelineForVideoBorders =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);


  // For drawing icons and animated voice bar. Add UI to the command encoder.
  let index_voice_bar = 0;
  function addUICommands(passEncoder) {
    // Icons
    passEncoder.setPipeline(pipelineForIcons);
    passEncoder.setVertexBuffer(0, verticesBufferForIcons);
    passEncoder.draw(videos.length * 6);

    // Animated voice bar on the last video.
    index_voice_bar++;
    if (index_voice_bar >= 10)
      index_voice_bar = 0;

    passEncoder.setPipeline(pipelineForAnimation);
    passEncoder.setVertexBuffer(0, vertexBufferForAnimation);
    passEncoder.draw(
        /*vertexCount=*/ 6, 1, /*firstVertex=*/ index_voice_bar * 6);

    // Borders of the last video
    // Is there a way to set the line width?
    passEncoder.setPipeline(pipelineForVideoBorders);
    passEncoder.setVertexBuffer(0, vertexBufferForAnimation);
    // vertexCount = 4 lines * 2 vertices = 8;
    // firstVertex = the end of the voice bar vetices =
    // 10 steps * 6 vertices = 60;
    passEncoder.draw(/*vertexCount=*/ 8, 1, /*firstVertex=*/ 60);
  }

  // videos #0-#3 : 30 fps.
  // videos #3-#15: 15 fps.
  // videos #16+: 7 fps.
  // Not every video frame is ready in rAF callback. Only draw videos that
  // are ready.
  var videoIsReady = new Array(videos.length);

  function UpdateIsVideoReady(video) {
    videoIsReady[video.id] = true;
    video.requestVideoFrameCallback(function () {
      UpdateIsVideoReady(video);
    });
  }

  for (const video of videos) {
    video.requestVideoFrameCallback(function () {
      UpdateIsVideoReady(video);
    });
  }

  // If rAF is running at 60 fps, skip every other frame so the demo is
  // running at 30 fps.
  // 30 fps is 33 milliseconds/frame. To prevent skipping frames accidentally
  // when rAF is running near 30fps with small delta, use 32 ms instead of 33 ms
  // for comparison.
  const frameTime30Fps = 32;
  let lastTimestamp = performance.now();

  const oneFrame = () => {
    const timestamp = performance.now();
    const elapsed = timestamp - lastTimestamp;
    if (elapsed < frameTime30Fps) {
      window.requestAnimationFrame(oneFrame);
      return;
    }
    lastTimestamp = timestamp;

    const swapChainTexture = context.getCurrentTexture();
    renderPassDescriptor.colorAttachments[0].view = swapChainTexture
      .createView();

    const commandEncoder = device.createCommandEncoder();

    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setVertexBuffer(0, verticesBuffer);

    Promise.all(videos.map(video =>
      (videoIsReady[video.id] ? createImageBitmap(video) : null))).
      then((videoFrames) => {
        for (let i = 0; i < videos.length; ++i) {
          if (videoFrames[i] != undefined) {
            device.queue.copyExternalImageToTexture(
                {source: videoFrames[i], origin: {x: 0, y: 0}},
                {texture: videoTextures[i]},
                {
                  width: videos[i].videoWidth,
                  height: videos[i].videoHeight,
                  depthOrArrayLayers: 1
                },
            );
            videoIsReady[i] = false;
          }
        }

        for (let i = 0; i < videos.length; ++i) {
          const firstVertex = i * 6;
          passEncoder.setBindGroup(0, bindGroups[i]);
          passEncoder.draw(6, 1, firstVertex, 0);
        }

        // Add UI on Top of all videos.
        if (addUI) {
          addUICommands(passEncoder);
        }
        passEncoder.endPass();
        device.queue.submit([commandEncoder.finish()]);

        // TODO(crbug.com/1289482): Workaround for backpressure mechanism
        // not working properly.
        device.queue.onSubmittedWorkDone().then(() => {
          window.requestAnimationFrame(oneFrame);
        });
      });
  };

  const oneFrameWithImportTextureApi = () => {
    // Target frame rate: 30 fps. rAF might run at 60 fps.
    const timestamp = performance.now();
    const elapsed = timestamp - lastTimestamp;
    if (elapsed < frameTime30Fps) {
      window.requestAnimationFrame(oneFrameWithImportTextureApi);
      return;
    }
    lastTimestamp = timestamp;

    // Always import all videos. The video textures are destroyed before the
    // next frame.
    for (let i = 0; i < videos.length; ++i) {
      videoTextures[i] =
          device.importExternalTexture(externalTextureDescriptor[i]);
    }

    const swapChainTexture = context.getCurrentTexture();
    renderPassDescriptor.colorAttachments[0].view = swapChainTexture
      .createView();

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
            resource: videoTextures[i],
          },
        ],
      });
      const firstVertex = i * 6;
      passEncoder.setBindGroup(0, bindGroups[i]);
      passEncoder.draw(6, 1, firstVertex, 0);
    }

    // Add UI on Top of all videos.
    if (addUI) {
      addUICommands(passEncoder);
    }
    passEncoder.endPass();
    device.queue.submit([commandEncoder.finish()]);

    const functionDuration = performance.now() - timestamp;
    const interval30Fps = 1000.0 / 30;  // 33.3 ms.
    if (functionDuration > interval30Fps) {
      console.warn(
          'rAF callback oneFrameWithImportTextureApi() takes ',
          functionDuration, 'ms,  longer than 33.3 ms (1sec/30fps)');
    }

    // TODO(crbug.com/1289482): Workaround for backpressure mechanism
    // not working properly.
    device.queue.onSubmittedWorkDone().then(() => {
      window.requestAnimationFrame(oneFrameWithImportTextureApi);
    });
  };

  if (useImportTextureApi) {
    window.requestAnimationFrame(oneFrameWithImportTextureApi);
  } else {
    window.requestAnimationFrame(oneFrame);
  }
}
