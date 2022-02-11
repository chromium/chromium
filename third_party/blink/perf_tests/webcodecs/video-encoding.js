function fourColorsFrame(ctx, width, height, text) {
  const kYellow = "#FFFF00";
  const kRed = "#FF0000";
  const kBlue = "#0000FF";
  const kGreen = "#00FF00";

  ctx.fillStyle = kYellow;
  ctx.fillRect(0, 0, width / 2, height / 2);

  ctx.fillStyle = kRed;
  ctx.fillRect(width / 2, 0, width / 2, height / 2);

  ctx.fillStyle = kBlue;
  ctx.fillRect(0, height / 2, width / 2, height / 2);

  ctx.fillStyle = kGreen;
  ctx.fillRect(width / 2, height / 2, width / 2, height / 2);

  ctx.fillStyle = 'white';
  ctx.font = (height / 10) + 'px sans-serif';
  ctx.fillText(text, width / 2, height / 2);
}

function prepareFrames(width, height, count) {
  const canvas = new OffscreenCanvas(width, height);
  const ctx = canvas.getContext('2d');
  const duration = 1_000_000 / 30;  // 1/30 s
  let timestamp = 0;
  const frames = [];
  for (let i = 0; i < count; i++) {
    fourColorsFrame(ctx, width, height, timestamp.toString());
    let frame = new VideoFrame(canvas, {timestamp: timestamp});
    frames.push(frame);
    timestamp += duration;
  }
  return frames;
}

async function testEncodingConfiguration(name, width, height, count, acc) {
  const encoder_config = {
    codec: "avc1.42001E",
    hardwareAcceleration: acc,
    width: width,
    height: height,
    bitrate: 2000000,
    framerate: 30
  };

  let support = await VideoEncoder.isConfigSupported(encoder_config);
  if (!support.supported) {
    testRunner.notifyDone();
    return;
  }

  let frames = prepareFrames(width, height, count);
  let is_done = false;

  const init = {
    output(chunk, metadata) {},
    error(e) {
      PerfTestRunner.logFatalError("Encoding error: " + e);
    }
  };

  async function runTest() {
    const encoder = new VideoEncoder(init);
    encoder.configure(encoder_config);

    PerfTestRunner.addRunTestStartMarker();

    // Encode first frame without timing it, this will given the encoder
    // chance to finish initialization.
    encoder.encode(frames[0], { keyFrame: false });
    await encoder.flush().catch(e => {
      PerfTestRunner.logFatalError("Test error: " + e);
    });

    let start_time = PerfTestRunner.now();
    for (let frame of frames.slice(1)) {
      encoder.encode(frame, { keyFrame: false });
    }

    encoder.flush().then(
     _ => {
      let run_time = PerfTestRunner.now() - start_time;
      PerfTestRunner.measureValueAsync(run_time);
      PerfTestRunner.addRunTestEndMarker();
      encoder.close();
      if (!is_done)
        runTest();
    },
    e => {
      PerfTestRunner.logFatalError("Test error: " + e);
    });
  }

  PerfTestRunner.startMeasureValuesAsync({
        unit: 'ms',
        done: function () {
          is_done = true;
          for (let frame of frames)
            frame.close();
        },
        run: function() {
            runTest();
        },
        warmUpCount: 0,
        iterationCount: 3,
        description: name,
  });
}
