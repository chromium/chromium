// Copyright 2021 The Chromium Authors
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
  @builtin(position) Position : vec4<f32>,
  @location(0) fragUV : vec2<f32>,
}

@vertex fn main(
  @location(0) position : vec2<f32>,
  @location(1) uv : vec2<f32>
) -> VertexOutput {
  var output : VertexOutput;
  output.Position = vec4<f32>(position, 0.0, 1.0);
  output.fragUV = uv;
  return output;
}
`,

  fragmentExternalTexture: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_external;

@fragment
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSampleBaseClampToEdge(myTexture, mySampler, fragUV);
}
`,

  fragment: `
@group(0) @binding(0) var mySampler: sampler;
@group(0) @binding(1) var myTexture: texture_2d<f32>;

@fragment
fn main(@location(0) fragUV : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(myTexture, mySampler, fragUV);
}
`,

  vertexIcons: `
@vertex
fn main(@location(0) position : vec2<f32>)
    -> @builtin(position) vec4<f32> {
  return vec4<f32>(position, 0.0, 1.0);
}
`,

  fragmentOutputBlue: `
@fragment
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(0.11328125, 0.4296875, 0.84375, 1.0);
}
`,
  fragmentOutputLightBlue: `
@fragment
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(0.3515625, 0.50390625, 0.75390625, 1.0);
}
`,

  fragmentOutputWhite: `
@fragment
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
`,
};

function createVertexBufferForVideos(device, videos, videoRows, videoColumns) {
  const rectVerts = getArrayForVideoVertexBuffer(videos, videoRows,
    videoColumns);
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
  const rectVerts = getArrayForIconVertexBuffer(videos, videoRows,
    videoColumns);
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
  const rectVerts = getArrayForAnimationVertexBuffer(videos, videoRows,
    videoColumns);
  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });

  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  return verticesBuffer;
}

function createVertexBufferForFPS(device) {
  const rectVerts = getArrayForFPSVertexBuffer(fpsPanels.length);
  const verticesBuffer = device.createBuffer({
    size: rectVerts.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });

  new Float32Array(verticesBuffer.getMappedRange()).set(rectVerts);
  verticesBuffer.unmap();

  return verticesBuffer;
}

function webGpuDrawVideoFrames(
    gpuSetting, videos, videoRows, videoColumns, addUI, addFPS,
    useImportTextureApi, capUIFPS, enableBackPressureWorkaround) {
  initializeFPSPanels();

  const {device, context} = gpuSetting;

  const vertexBufferForVideos = createVertexBufferForVideos(device, videos,
    videoRows, videoColumns);

  const swapChainFormat = navigator.gpu.getPreferredCanvasFormat();

  context.configure({
    device,
    format: swapChainFormat,
    usage: GPUTextureUsage.RENDER_ATTACHMENT,
    alphaMode: 'opaque'
  });

  function getVideoPipeline(fragmentShaderModule) {
    return device.createRenderPipeline({
      layout: 'auto',
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
  }

  const pipelineForVideos = getVideoPipeline(device.createShaderModule({
    code: wgslShaders.fragment,
  }));

  const externalTexturesPipeline = getVideoPipeline(device.createShaderModule({
    code: wgslShaders.fragmentExternalTexture,
  }));

  const renderPassDescriptorForVideo = {
    colorAttachments: [
      {
        view: undefined, // Assigned later
        clearValue: { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
        loadOp: 'clear',
        storeOp: 'store',
      },
    ],
  };

  const sampler = device.createSampler({
    magFilter: 'linear',
    minFilter: 'linear',
  });

  const videoTextures = [];
  const videoBindGroups = [];

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

      videoBindGroups[i] = device.createBindGroup({
        layout: pipelineForVideos.getBindGroupLayout(0),
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
    layout: 'auto',
    vertex: {
      module: device.createShaderModule({
        code: wgslShaders.vertexIcons,
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
    code: wgslShaders.fragmentOutputBlue,
  });
  const pipelineForIcons =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);

  // For rendering the voice bar animation
  const vertexBufferForAnimation =
      createVertexBufferForAnimation(device, videos, videoRows, videoColumns);

  renderPipelineDescriptorForIcon.fragment.module = device.createShaderModule({
    code: wgslShaders.fragmentOutputWhite,
  });
  const pipelineForAnimation =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);

  // For rendering the borders of the last video
  renderPipelineDescriptorForIcon.fragment.module = device.createShaderModule({
    code: wgslShaders.fragmentOutputLightBlue,
  });
  renderPipelineDescriptorForIcon.primitive.topology = 'line-list';
  const pipelineForVideoBorders =
      device.createRenderPipeline(renderPipelineDescriptorForIcon);

  const vertexBufferForFPS = createVertexBufferForFPS(device);
  const fpsTextures = [];
  const fpsBindGroups = [];
  for (let i = 0; i < fpsPanels.length; ++i) {
    fpsTextures[i] = device.createTexture({
      size: {
        width: fpsPanels[i].dom.width,
        height: fpsPanels[i].dom.height,
        depthOrArrayLayers: 1,
      },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.RENDER_ATTACHMENT,
    });

    fpsBindGroups[i] = device.createBindGroup({
      // Re-use the video program to draw FPS panels.
      layout: pipelineForVideos.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: sampler,
        },
        {
          binding: 1,
          resource: fpsTextures[i].createView(),
        },
      ],
    });
  }

  // For drawing icons and animated voice bar. Add UI to the command encoder.
  let indexVoiceBar = 0;
  function addUICommands(passEncoder) {
    // Icons
    passEncoder.setPipeline(pipelineForIcons);
    passEncoder.setVertexBuffer(0, verticesBufferForIcons);
    passEncoder.draw(videos.length * 6);

    // Animated voice bar on the last video.
    indexVoiceBar++;
    if (indexVoiceBar >= 10) {
      indexVoiceBar = 0;
    }

    passEncoder.setPipeline(pipelineForAnimation);
    passEncoder.setVertexBuffer(0, vertexBufferForAnimation);
    passEncoder.draw(
        /*vertexCount=*/ 6, 1, /*firstVertex=*/ indexVoiceBar * 6);

    // Borders of the last video
    // Is there a way to set the line width?
    passEncoder.setPipeline(pipelineForVideoBorders);
    passEncoder.setVertexBuffer(0, vertexBufferForAnimation);
    // vertexCount = 4 lines * 2 vertices = 8;
    // firstVertex = the end of the voice bar vertices =
    // 10 steps * 6 vertices = 60;
    passEncoder.draw(/*vertexCount=*/ 8, 1, /*firstVertex=*/ 60);
  }

  function addFPSCommands(device, passEncoder) {
    // FPS Panels
    // Re-use the video program to draw FPS panels.
    passEncoder.setPipeline(pipelineForVideos);
    passEncoder.setVertexBuffer(0, vertexBufferForFPS);
    for (let i = 0; i < fpsPanels.length; ++i) {
      device.queue.copyExternalImageToTexture(
          {source: fpsPanels[i].dom, origin: {x: 0, y: 0}},
          {texture: fpsTextures[i]},
          {
            width: fpsPanels[i].dom.width,
            height: fpsPanels[i].dom.height,
            depthOrArrayLayers: 1
          },
      );
      const firstVertex = i * 6;
      passEncoder.setBindGroup(0, fpsBindGroups[i]);
      passEncoder.draw(6, 1, firstVertex, 0);
    }
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

  let lastTimestamp = performance.now();

  const oneFrame = async () => {
    // Target frame rate: 30 fps when capUIFPS is true.
    // Normally rAF runs at the refresh rate of the system/monitor.
    const timestamp = performance.now();
    if (capUIFPS) {
      const elapsed = timestamp - lastTimestamp;
      if (elapsed < kFrameTime30Fps) {
        window.requestAnimationFrame(oneFrame);
        return;
      }
      lastTimestamp = timestamp;
    }

    uiFrames++;

    const swapChainTexture = context.getCurrentTexture();
    renderPassDescriptorForVideo.colorAttachments[0].view = swapChainTexture
      .createView();

    const commandEncoder = device.createCommandEncoder();

    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptorForVideo);
    passEncoder.setPipeline(pipelineForVideos);
    passEncoder.setVertexBuffer(0, vertexBufferForVideos);

    const videoFrames = await Promise.all(videos.map(video => {
      if (videoIsReady[video.id]) {
        return video;
      }
    }));

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
        totalVideoFrames++;
      }
    }

    for (let i = 0; i < videos.length; ++i) {
      const firstVertex = i * 6;
      passEncoder.setBindGroup(0, videoBindGroups[i]);
      passEncoder.draw(6, 1, firstVertex, 0);
    }

    // Add UI on Top of all videos.
    if (addUI) {
      addUICommands(passEncoder);
    }
    // Add FPS panels on Top of all videos.
    if (addFPS) {
      updateFPS(timestamp, videos);
      addFPSCommands(device, passEncoder);
    }
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);

    const functionDuration = performance.now() - timestamp;
    const interval30Fps = 1000.0 / 30;  // 33.3 ms.
    if (functionDuration > interval30Fps) {
      console.warn(`rAF callback oneFrame() takes ${
          functionDuration}ms,  longer than 33.3 ms (1sec/30fps)`);
    }

    // TODO(crbug.com/40817530): Workaround for backpressure mechanism
    // not working properly.
    if (enableBackPressureWorkaround) {
      await device.queue.onSubmittedWorkDone();
    }

    window.requestAnimationFrame(oneFrame);
  };

  const oneFrameWithImportTextureApi = async () => {
    // Target frame rate: 30 fps when capUIFPS is true.
    // Normally rAF runs at the refresh rate of the system/monitor.
    const timestamp = performance.now();
    if (capUIFPS) {
      const elapsed = timestamp - lastTimestamp;
      if (elapsed < kFrameTime30Fps) {
        window.requestAnimationFrame(oneFrameWithImportTextureApi);
        return;
      }
      lastTimestamp = timestamp;
    }

    uiFrames++;

    for (let i = 0; i < videos.length; ++i) {
      videoTextures[i] =
          device.importExternalTexture(externalTextureDescriptor[i]);
      if (videoIsReady[i]) {
        totalVideoFrames++;
      }
    }

    const swapChainTexture = context.getCurrentTexture();
    renderPassDescriptorForVideo.colorAttachments[0].view = swapChainTexture
      .createView();

    const commandEncoder = device.createCommandEncoder();
    const passEncoder =
      commandEncoder.beginRenderPass(renderPassDescriptorForVideo);
    passEncoder.setPipeline(externalTexturesPipeline);
    passEncoder.setVertexBuffer(0, vertexBufferForVideos);

    for (let i = 0; i < videos.length; ++i) {
      videoBindGroups[i] = device.createBindGroup({
        layout: externalTexturesPipeline.getBindGroupLayout(0),
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
      videoIsReady[i] = false;
      const firstVertex = i * 6;
      passEncoder.setBindGroup(0, videoBindGroups[i]);
      passEncoder.draw(6, 1, firstVertex, 0);
    }

    // Add UI on Top of all videos.
    if (addUI) {
      addUICommands(passEncoder);
    }
    // Add FPS panels on Top of all videos.
    if (addFPS) {
      updateFPS(timestamp, videos);
      addFPSCommands(device, passEncoder);
    }
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);

    const functionDuration = performance.now() - timestamp;
    const interval30Fps = 1000.0 / 30;  // 33.3 ms.
    if (functionDuration > interval30Fps) {
      console.warn(`rAF callback oneFrameWithImportTextureApi() takes ${
          functionDuration}ms,  longer than 33.3 ms (1sec/30fps)`);
    }

    // TODO(crbug.com/40817530): Workaround for backpressure mechanism
    // not working properly.
    if (enableBackPressureWorkaround) {
      await device.queue.onSubmittedWorkDone();
    }
    window.requestAnimationFrame(oneFrameWithImportTextureApi);
  };

  if (useImportTextureApi) {
    window.requestAnimationFrame(oneFrameWithImportTextureApi);
  } else {
    window.requestAnimationFrame(oneFrame);
  }
}
