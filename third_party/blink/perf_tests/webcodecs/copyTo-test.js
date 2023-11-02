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
