// META: global=window,dedicatedworker

// VideoFrames constructed from a buffer read each plane at the offset and
// stride given by the layout.

const kWidth = 6;
const kHeight = 4;
const kChroma = {width: kWidth / 2, height: kHeight / 2};

// The byte size of each plane's rows. NV12 interleaves U and V in its second
// plane. RGBX and BGRX are left out: copyTo() may make their fourth byte
// opaque, so they can't be compared byte for byte.
const kFormats = [
  {format: 'I420', planes: [{width: kWidth, height: kHeight}, kChroma, kChroma]},
  {
    format: 'I420A',
    planes: [
      {width: kWidth, height: kHeight}, kChroma, kChroma,
      {width: kWidth, height: kHeight}
    ]
  },
  {
    format: 'NV12',
    planes: [
      {width: kWidth, height: kHeight},
      {width: 2 * kChroma.width, height: kChroma.height}
    ]
  },
  {format: 'RGBA', planes: [{width: 4 * kWidth, height: kHeight}]},
  {format: 'BGRA', planes: [{width: 4 * kWidth, height: kHeight}]},
];

function makePlanes(info) {
  return info.planes.map((size, plane) => {
    const bytes = [];
    for (let i = 0; i < size.width * size.height; ++i)
      bytes.push((plane * 61 + i * 7 + 1) & 0xff);
    return {...size, bytes};
  });
}

function tightLayout(planes) {
  let offset = 0;
  return planes.map(plane => {
    const layout = {offset, stride: plane.width};
    offset += plane.width * plane.height;
    return layout;
  });
}

// Pads every row, and leaves a gap before and between planes.
function paddedLayout(planes, gap) {
  let offset = gap;
  return planes.map(plane => {
    const layout = {offset, stride: plane.width + 3};
    offset += layout.stride * plane.height + gap;
    return layout;
  });
}

function reversedLayout(planes) {
  let end = planes.reduce((total, plane) => total + plane.width * plane.height, 0);
  return planes.map(plane => {
    end -= plane.width * plane.height;
    return {offset: end, stride: plane.width};
  });
}

function byteLength(layout, planes) {
  return Math.max(...layout.map(
      (plane, index) => plane.offset + plane.stride * planes[index].height));
}

function writePlanes(planes, layout) {
  const data = new Uint8Array(byteLength(layout, planes)).fill(0xa5);
  planes.forEach((plane, index) => {
    for (let y = 0; y < plane.height; ++y) {
      data.set(
          plane.bytes.slice(y * plane.width, (y + 1) * plane.width),
          layout[index].offset + y * layout[index].stride);
    }
  });
  return data;
}

function readPlanes(data, layout, planes) {
  return planes.map((plane, index) => {
    const bytes = [];
    for (let y = 0; y < plane.height; ++y) {
      const start = layout[index].offset + y * layout[index].stride;
      bytes.push(...data.slice(start, start + plane.width));
    }
    return bytes;
  });
}

async function testRoundTrip(info, sourceLayout) {
  const planes = makePlanes(info);
  const frame = new VideoFrame(writePlanes(planes, sourceLayout), {
    format: info.format,
    codedWidth: kWidth,
    codedHeight: kHeight,
    timestamp: 0,
    layout: sourceLayout
  });
  assert_equals(frame.format, info.format, 'format');
  const destinationLayout = tightLayout(planes);
  const destination = new Uint8Array(byteLength(destinationLayout, planes));
  await frame.copyTo(destination);
  frame.close();
  readPlanes(destination, destinationLayout, planes)
      .forEach(
          (bytes, index) =>
              assert_array_equals(bytes, planes[index].bytes, `plane ${index}`));
}

for (const info of kFormats) {
  promise_test(
      () => testRoundTrip(info, paddedLayout(makePlanes(info), 0)),
      `${info.format}: padded rows`);
  promise_test(
      () => testRoundTrip(info, paddedLayout(makePlanes(info), 5)),
      `${info.format}: padded rows with gaps before and between planes`);
  if (info.planes.length > 1) {
    promise_test(
        () => testRoundTrip(info, reversedLayout(makePlanes(info))),
        `${info.format}: planes in reverse order`);
  }
}
