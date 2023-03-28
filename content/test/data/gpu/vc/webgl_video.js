// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function webglInit(canvasWidth, canvasHeight, mode = 'webgl') {
  const container = document.getElementById('container');
  const canvas = container.appendChild(document.createElement('canvas'));
  canvas.width = canvasWidth;
  canvas.height = canvasHeight;

  const gl = canvas.getContext(mode);
  if (!gl) {
    console.warn('WebGL not supported. canvas.getContext("webgl") fails!');
    return null;
  }

  return gl;
}

const webglShaders = {
  vertex: `
attribute vec2 aVertPos;
attribute vec2 aTexCoord;
varying mediump vec2 vTexCoord;
void main(void) {
  gl_Position = vec4(aVertPos, 0.0, 1.0);
  vTexCoord = aTexCoord;
}
`,

  fragment: `
precision mediump float;
varying mediump vec2 vTexCoord;
uniform sampler2D uSampler;
void main(void) {
  gl_FragColor = texture2D(uSampler, vTexCoord);
}
`,

  vertexIcons: `
attribute vec2 aVertPos;
void main(void) {
  gl_Position = vec4(aVertPos, 0.0, 1.0);
}
`,

  fragmentOutputBlue: `
void main(void) {
  gl_FragColor = vec4(0.11328125, 0.4296875, 0.84375, 1.0);
}
`,
  fragmentOutputLightBlue: `
void main(void) {
  gl_FragColor = vec4(0.3515625, 0.50390625, 0.75390625, 1.0);
}
`,

  fragmentOutputWhite: `
void main(void) {
  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
`,
};

function setupShader(gl, type, source) {
  const shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);

  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    console.log('An error occurred compiling the shaders: '
      + gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
    return null;
  }
  return shader;
}

function setupProgram(gl, vertexSource, fragmentSource) {
  const vertexShader = setupShader(gl, gl.VERTEX_SHADER, vertexSource);
  const fragmentShader = setupShader(gl, gl.FRAGMENT_SHADER, fragmentSource);

  const program = gl.createProgram();
  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.log('Unable to initialize the shader program: '
      + gl.getProgramInfoLog(program));
    return null;
  }
  return program;
}

function setupProgramForVideo(gl, vertexSource, fragmentSource) {
  const program = setupProgram(gl, vertexSource, fragmentSource);
  return program;
}

const webglPrograms = {
  video: null,
  icon: null,
  animation: null,
  border: null,
};

function initializePrograms(gl) {
  webglPrograms.video = setupProgramForVideo(gl, webglShaders.vertex,
    webglShaders.fragment);
  webglPrograms.icon = setupProgram(gl, webglShaders.vertexIcons,
    webglShaders.fragmentOutputBlue);
  webglPrograms.animation = setupProgram(gl, webglShaders.vertexIcons,
    webglShaders.fragmentOutputWhite);
  webglPrograms.border = setupProgram(gl, webglShaders.vertexIcons,
    webglShaders.fragmentOutputLightBlue);
}

function createVertexBufferForVideos(gl, videos, videoRows, videoColumns) {
  const rectVerts = getArrayForVideoVertexBuffer(videos, videoRows,
    videoColumns);
  const verticesBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, verticesBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, rectVerts, gl.STATIC_DRAW);

  return verticesBuffer;
}

function bindVertexBufferForTextureQuad(gl, vertexBuffer) {
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
  const pos = gl.getAttribLocation(webglPrograms.video, "aVertPos");
  gl.vertexAttribPointer(pos, 2, gl.FLOAT, false, 16, 0);
  gl.enableVertexAttribArray(pos);

  const coord = gl.getAttribLocation(webglPrograms.video, "aTexCoord");
  gl.vertexAttribPointer(coord, 2, gl.FLOAT, false, 16, 8);
  gl.enableVertexAttribArray(coord);
}

function bindVertexBufferForVideos(gl) {
  bindVertexBufferForTextureQuad(gl, webglVertexBuffers.video);
}

function createVertexBufferForIcons(gl, videos, videoRows, videoColumns) {
  const rectVerts = getArrayForIconVertexBuffer(videos, videoRows,
    videoColumns);
  const verticesBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, verticesBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, rectVerts, gl.STATIC_DRAW);

  return verticesBuffer;
}

function bindVertexBufferForIcons(gl) {
  gl.bindBuffer(gl.ARRAY_BUFFER, webglVertexBuffers.icon);
  const pos = gl.getAttribLocation(webglPrograms.icon, "aVertPos");
  gl.vertexAttribPointer(pos, 2, gl.FLOAT, false, 8, 0);
  gl.enableVertexAttribArray(pos);
}

function createVertexBufferForAnimation(gl, videos, videoRows, videoColumns) {
  const rectVerts = getArrayForAnimationVertexBuffer(videos, videoRows,
    videoColumns);
  const verticesBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, verticesBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, rectVerts, gl.STATIC_DRAW);

  return verticesBuffer;
}

function bindVertexBufferForAnimation(gl) {
  gl.bindBuffer(gl.ARRAY_BUFFER, webglVertexBuffers.animation);
  let pos = gl.getAttribLocation(webglPrograms.animation, "aVertPos");
  gl.vertexAttribPointer(pos, 2, gl.FLOAT, false, 8, 0);
  gl.enableVertexAttribArray(pos);

  pos = gl.getAttribLocation(webglPrograms.border, "aVertPos");
  gl.vertexAttribPointer(pos, 2, gl.FLOAT, false, 8, 0);
  gl.enableVertexAttribArray(pos);
}

function createVertexBufferForFPS(gl) {
  const rectVerts = getArrayForFPSVertexBuffer(fpsPanels.length);
  const verticesBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, verticesBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, rectVerts, gl.STATIC_DRAW);

  return verticesBuffer;
}

function bindVertexBufferForFPS(gl) {
  bindVertexBufferForTextureQuad(gl, webglVertexBuffers.fps);
}

const webglVertexBuffers = {
  video: null,
  icon: null,
  animation: null,
};

function initializeVertexBuffers(gl, videos, videoRows, videoColumns, addFPS) {
  webglVertexBuffers.video = createVertexBufferForVideos(gl, videos, videoRows,
    videoColumns);
  webglVertexBuffers.icon = createVertexBufferForIcons(gl, videos, videoRows,
    videoColumns);
  webglVertexBuffers.animation = createVertexBufferForAnimation(gl, videos,
    videoRows, videoColumns);
  if (addFPS) {
    webglVertexBuffers.fps = createVertexBufferForFPS(gl);
  }
}

function initTexture(gl) {
  const texture = gl.createTexture();
  gl.activeTexture(gl.TEXTURE0);
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  return texture;
}

const webglVideoTextures = [];
function initializeVideoTextures(gl, count) {
  for (let i = 0; i < count; ++i) {
    const texture = initTexture(gl);
    webglVideoTextures.push({texture});
  }
}

function updateTextureViaTexStorage2D(
    gl, textureInfo, source, texWidth, texHeight) {
  if (textureInfo.texWidth !== texWidth ||
      textureInfo.texHeight !== texHeight) {
    gl.deleteTexture(textureInfo.texture);
    textureInfo.texture = initTexture(gl);
    textureInfo.texWidth = texWidth;
    textureInfo.texHeight = texHeight;
    gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, texWidth, texHeight);
    console.log(`Reinitializing texture (${texWidth}x${texHeight})`);
  } else {
    gl.activeTexture(gl.TEXTURE0);
  }

  texture = textureInfo.texture;
  gl.bindTexture(gl.TEXTURE_2D, texture);

  gl.texSubImage2D(
      gl.TEXTURE_2D, 0, 0, 0, texWidth, texHeight, gl.RGBA, gl.UNSIGNED_BYTE,
      source);
}

function updateTextureViaTexImage2D(gl, textureInfo, source) {
  if (textureInfo.texWidth || textureInfo.texHeight) {
    gl.deleteTexture(textureInfo.texture);
    textureInfo.texture = initTexture(gl);
    textureInfo.texWidth = 0;
    textureInfo.texHeight = 0;
    console.log('Reinitializing texture');
  } else {
    gl.activeTexture(gl.TEXTURE0);
  }

  texture = textureInfo.texture;
  gl.bindTexture(gl.TEXTURE_2D, texture);
  const level = 0;
  const internalFormat = gl.RGBA;
  const srcFormat = gl.RGBA;
  const srcType = gl.UNSIGNED_BYTE;
  gl.texImage2D(
      gl.TEXTURE_2D, level, internalFormat, srcFormat, srcType, source);
}

const webglFPSTextures = [];
function initializeFPSTextures(gl, count) {
  for (let i = 0; i < count; ++i) {
    const texture = initTexture(gl);
    webglFPSTextures.push({texture});
  }
}

function webglDrawVideoFrames(
    gl, videos, videoRows, videoColumns, addUI, addFPS, capUIFPS,
    fixedTextureSize) {
  initializePrograms(gl);
  initializeVideoTextures(gl, videos.length);
  if (addFPS) {
    initializeFPSPanels();
    initializeFPSTextures(gl, fpsPanels.length);
  }
  initializeVertexBuffers(gl, videos, videoRows, videoColumns, addFPS);

  // videos #0-#3 : 30 fps.
  // videos #3-#15: 15 fps.
  // videos #16+: 7 fps.
  // Not every video frame is ready in rAF callback. Only draw videos that
  // are ready.
  var videoIsReady = new Array(videos.length);

  function updateIsVideoReady(video) {
    videoIsReady[video.id] = true;
    video.requestVideoFrameCallback(function () {
      updateIsVideoReady(video);
    });
  }

  for (const video of videos) {
    video.requestVideoFrameCallback(function () {
      updateIsVideoReady(video);
    });
  }

  let lastTimestamp = performance.now();
  let index_voice_bar = 0;

  const oneFrame = () => {
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

    gl.clearColor(1.0, 1.0, 1.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.useProgram(webglPrograms.video);
    gl.uniform1i(gl.getUniformLocation(webglPrograms.video, "uSampler"), 0);
    bindVertexBufferForVideos(gl);
    for (let i = 0; i < videos.length; ++i) {
      if (videoIsReady[videos[i].id]) {
        if (fixedTextureSize) {
          updateTextureViaTexStorage2D(
              gl, webglVideoTextures[i], videos[i], videos[i].videoWidth,
              videos[i].videoHeight);
        } else {
          updateTextureViaTexImage2D(gl, webglVideoTextures[i], videos[i]);
        }
        videoIsReady[videos[i].id] = false;
        totalVideoFrames++;
      }
    }

    for (let i = 0; i < videos.length; ++i) {
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, webglVideoTextures[i].texture);
      gl.drawArrays(gl.TRIANGLES, 6 * i, 6);
    }
    // Add UI on Top of all videos.
    if (addUI) {
      gl.useProgram(webglPrograms.icon);
      bindVertexBufferForIcons(gl);
      gl.drawArrays(gl.TRIANGLES, 0, 6 * videos.length);

      // Animated voice bar on the last video.
      index_voice_bar++;
      if (index_voice_bar >= 10) {
        index_voice_bar = 0;
      }
      gl.useProgram(webglPrograms.animation);
      bindVertexBufferForAnimation(gl);
      gl.drawArrays(gl.TRIANGLES, index_voice_bar * 6, 6);

      gl.useProgram(webglPrograms.border);
      gl.bindBuffer(gl.ARRAY_BUFFER, webglVertexBuffers.animation);
      gl.drawArrays(gl.LINES, 60, 8);
    }

    if (addFPS) {
      updateFPS(timestamp, videos);
      // Re-use the video program to draw FPS panels.
      gl.useProgram(webglPrograms.video);

      bindVertexBufferForFPS(gl);
      for (let i = 0; i < fpsPanels.length; ++i) {
        updateTextureViaTexImage2D(gl, webglFPSTextures[i], fpsPanels[i].dom);
        gl.drawArrays(gl.TRIANGLES, 6 * i, 6);
      }
    }

    window.requestAnimationFrame(oneFrame);
  };

  window.requestAnimationFrame(oneFrame);
}
