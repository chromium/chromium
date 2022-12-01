function runCopyToTest(frame, desc) {
  let isDone = false;

  function runTest() {
    let size = frame.allocationSize();
    let buf = new ArrayBuffer(size);
    let startTime = PerfTestRunner.now();
    PerfTestRunner.addRunTestStartMarker();
    frame.copyTo(buf)
        .then(layout => {
          PerfTestRunner.measureValueAsync(PerfTestRunner.now() - startTime);
          PerfTestRunner.addRunTestEndMarker();
          if (!isDone)
            runTest();
        })
        .catch(e => {
          PerfTestRunner.logFatalError('Test error: ' + e);
        })
  }

  PerfTestRunner.startMeasureValuesAsync({
    description: desc,
    unit: 'ms',
    done: _ => {
      isDone = true;
      frame.close();
    },
    run: _ => {
      runTest();
    },
  });
}

function runBatchCopyToTest(frames, desc) {
  let isDone = false;

  function runTest() {
    let startTime = PerfTestRunner.now();
    PerfTestRunner.addRunTestStartMarker();

    let frames_and_buffers = frames.map(frame => {
      let size = frame.allocationSize();
      let buf = new ArrayBuffer(size);
      return [frame, buf];
    });
    let readback_promises = frames_and_buffers.map(([frame, buf]) => {
      return frame.copyTo(buf);
    });
    Promise.all(readback_promises)
        .then(layouts => {
          PerfTestRunner.measureValueAsync(PerfTestRunner.now() - startTime);
          PerfTestRunner.addRunTestEndMarker();
          if (!isDone)
            runTest();
        })
        .catch(e => {
          PerfTestRunner.logFatalError('Test error: ' + e);
        })
  }

  PerfTestRunner.startMeasureValuesAsync({
    description: desc,
    unit: 'ms',
    done: _ => {
      isDone = true;
      for (let frame of frames)
        frame.close();
    },
    run: _ => {
      runTest();
    },
  });
}