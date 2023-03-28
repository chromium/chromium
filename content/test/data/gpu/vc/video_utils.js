// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const parsedString = (function (names) {
  const pairs = {};
  for (let i = 0; i < names.length; ++i) {
    var keyValue = names[i].split('=', 2);
    if (keyValue.length == 1)
      pairs[keyValue[0]] = '';
    else
      pairs[keyValue[0]] = decodeURIComponent(keyValue[1].replace(/\+/g, ' '));
  }
  return pairs;
})(window.location.search.substr(1).split('&'));

function GetVideoSource(videoCount, index, codec, useLargeSizeVideo = false) {
  switch (codec) {
    case 'vp8':
      if (videoCount <= 4 || useLargeSizeVideo) {
        return './teddy1_vp8_640x360_30fps.webm';
      } else {
        if (index < 4) {
          return './teddy3_vp8_320x180_30fps.webm';
        } else if (index < 16) {
          return './teddy2_vp8_320x180_15fps.webm';
        } else {
          return './teddy1_vp8_320x180_7fps.webm';
        }
      }
      break;

    case 'vp9':
    default:
      if (videoCount <= 4 || useLargeSizeVideo) {
        return './teddy1_vp9_640x360_30fps.webm';
      } else {
        if (index < 4) {
          return './teddy3_vp9_320x180_30fps.webm';
        } else if (index < 16) {
          return './teddy2_vp9_320x180_15fps.webm';
        } else {
          return './teddy1_vp9_320x180_7fps.webm';
        }
      }
      break;
  }
}

function getArrayForVideoVertexBuffer(videos, videoRows, videoColumns) {
  // Each video takes 6 vertices (2 triangles). Each vertex has 4 floats.
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

  return rectVerts;
}

function getArrayForIconVertexBuffer(videos, videoRows, videoColumns) {
  // Each icon takes 6 vertices (2 triangles). Each vertex has 2 floats.
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

  return rectVerts;
}

function getArrayForAnimationVertexBuffer(videos, videoRows, videoColumns) {
  // Create voice bar animation and borders of the last video.
  // (1) Generate 10 different lengths of voice bar. Each bar takes 2 triangles,
  // which are 6 vertices.
  // (2) Generate borders, consisting of 4 lines. Each line takes 2 vertices.
  // Each vertex has 2 floats.
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
    const hBar = (i + 1) * delta;
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

  return rectVerts;
}

function getArrayForFPSVertexBuffer(fpsCount) {
  // Each FPS takes 6 vertices (2 triangles). Each vertex has 4 floats.
  // Therefore, each FPS needs 24 floats.
  const rectVerts = new Float32Array(fpsCount * 24);

  const fpsRows = 16;
  const fpsColumns = 16;
  let w = 2.0 / fpsColumns;
  let h = 2.0 / fpsRows;
  for (let row = 0; row < fpsRows; ++row) {
    for (let column = 0; column < fpsColumns; ++column) {
      const count = (row * fpsColumns + column);
      if (count >= fpsCount) {
        return rectVerts;
      }
      const array_index = count * 24;
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

  return rectVerts;
}

const fpsPanels = [];
const kUiFPSPanel = 0;
const kVideoFPSPanel = 1;
function initializeFPSPanels() {
  fpsPanels.push(new Stats.Panel('UI', '#0ff', '#002'));
  fpsPanels.push(new Stats.Panel('Video', '#0f0', '#020'));
}

// If rAF is running at 60 fps, skip every other frame so the demo is
// running at 30 fps.
// 30 fps is 33 milliseconds/frame. To prevent skipping frames accidentally
// when rAF is running near 30fps, we use 5ms delta for jittering.
const kFrameTime30Fps = 33 - 5;

// The time of last FPS update.
let fpsPrevTime = performance.now();
// How many UI refreshments have been made since last update.
let uiFrames = 0;
// How many new video frames have been imported or copied since last update.
let totalVideoFrames = 0;

function updateFPS(timestamp, videos) {
  // Update every 1 second.
  if (timestamp >= fpsPrevTime + 1000) {
    const fpsElapsed = timestamp - fpsPrevTime;
    let fps = uiFrames * 1000 / fpsElapsed;
    fpsPanels[kUiFPSPanel].update(fps, kFrameTime30Fps);

    fps = totalVideoFrames * 1000 / fpsElapsed;
    // Average fps per video element.
    fps /= videos.length;
    fpsPanels[kVideoFPSPanel].update(fps, kFrameTime30Fps);

    fpsPrevTime = timestamp;
    uiFrames = 0;
    totalVideoFrames = 0;
  }
}
