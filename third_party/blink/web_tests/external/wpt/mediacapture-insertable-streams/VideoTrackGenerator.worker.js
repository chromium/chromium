// META: title=VideoTrackGenerator tests.

importScripts("/resources/testharness.js");

const pixelColour = [50, 100, 150, 255];
const height = 240;
const width = 320;
function makeVideoFrame(timestamp) {
  const canvas = new OffscreenCanvas(width, height);

  const ctx = canvas.getContext('2d', {alpha: false});
  ctx.fillStyle = `rgba(${pixelColour.join()})`;
  ctx.fillRect(0, 0, width, height);

  return new VideoFrame(canvas, {timestamp, alpha: 'discard'});
}

promise_test(async t => {
  const videoFrame = makeVideoFrame(1);
  const originalWidth = videoFrame.displayWidth;
  const originalHeight = videoFrame.displayHeight;
  const originalTimestamp = videoFrame.timestamp;
  const generator = new VideoTrackGenerator();
  t.add_cleanup(() => generator.track.stop());

  // Use a MediaStreamTrackProcessor as a sink for |generator| to verify
  // that |processor| actually forwards the frames written to its writable
  // field.
  const processor = new MediaStreamTrackProcessor(generator);
  const reader = processor.readable.getReader();
  const readerPromise = new Promise(async resolve => {
    const result = await reader.read();
    t.add_cleanup(() => result.value.close());
    t.step_func(() => {
      assert_equals(result.value.displayWidth, originalWidth);
      assert_equals(result.value.displayHeight, originalHeight);
      assert_equals(result.value.timestamp, originalTimestamp);
    })();
    resolve();
  });

  generator.writable.getWriter().write(videoFrame);
  return readerPromise;
}, 'Tests that VideoTrackGenerator forwards frames to sink');

promise_test(async t => {
  const generator = new VideoTrackGenerator();
  t.add_cleanup(() => generator.track.stop());

  const writer = generator.writable.getWriter();
  const frame = makeVideoFrame(1);
  await writer.write(frame);

  assert_equals(generator.track.kind, "video");
  assert_equals(generator.track.readyState, "live");
}, "Tests that creating a VideoTrackGenerator works as expected");

promise_test(async t => {
  const generator = new VideoTrackGenerator();
  t.add_cleanup(() => generator.track.stop());

  const writer = generator.writable.getWriter();
  const frame = makeVideoFrame(1);
  await writer.write(frame);

  assert_throws_dom("InvalidStateError", () => frame.clone(), "VideoFrame wasn't destroyed on write.");
}, "Tests that VideoFrames are destroyed on write.");

promise_test(async t => {
  const generator = new VideoTrackGenerator();
  t.add_cleanup(() => generator.track.stop());

  const writer = generator.writable.getWriter();
  const frame = makeVideoFrame(1);
  t.add_cleanup(() => frame.close());
  assert_throws_js(TypeError, writer.write(frame));
}, "Mismatched frame and generator kind throws on write.");

promise_test(async t => {
  const generator = new VideoTrackGenerator();
  t.add_cleanup(() => generator.track.stop());

  // Use a MediaStreamTrackProcessor as a sink for |generator| to verify
  // that |processor| actually forwards the frames written to its writable
  // field.
  const processor = new MediaStreamTrackProcessor(generator);
  const reader = processor.readable.getReader();
  const videoFrame = makeVideoFrame(1);

  const writer = generator.writable.getWriter();
  const videoFrame1 = makeVideoFrame(1);
  writer.write(videoFrame1);
  const result1 = await reader.read();
  t.add_cleanup(() => result1.value.close());
  assert_equals(result1.value.timestamp, 1);
  generator.muted = true;

  // This frame is expected to be discarded.
  const videoFrame2 = makeVideoFrame(2);
  writer.write(videoFrame2);
  generator.muted = false;

  const videoFrame3 = makeVideoFrame(3);
  writer.write(videoFrame3);
  const result3 = await reader.read();
  t.add_cleanup(() => result3.value.close());
  assert_equals(result3.value.timestamp, 3);

  // Set up a read ahead of time, then mute, enqueue and unmute.
  const promise5 = reader.read();
  generator.muted = true;
  writer.write(makeVideoFrame(4)); // Expected to be discarded.
  generator.muted = false;
  writer.write(makeVideoFrame(5));
  const result5 = await promise5;
  t.add_cleanup(() => result5.value.close());
  assert_equals(result5.value.timestamp, 5);
}, 'Tests that VideoTrackGenerator forwards frames only when unmuted');

done();
