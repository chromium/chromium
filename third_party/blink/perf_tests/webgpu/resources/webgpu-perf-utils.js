
function skipTest(message) {
  PerfTestRunner.log(message);

  const skip = () => {
    if (window.testRunner) {
      testRunner.notifyDone();
    }
  }

  if (window.testRunner && window.testRunner.telemetryIsRunning) {
    testRunner.waitForTelemetry([], skip);
  } else {
    skip();
  }
}

function loopTestForTime(minTime, test) {
  var isDone = false;
  async function runTest() {
    PerfTestRunner.addRunTestStartMarker();

    let numberOfTests = 0;
    const startTime = PerfTestRunner.now();
    let totalTime = 0;
    while (totalTime < minTime) {
      await test();
      numberOfTests++;
      totalTime = PerfTestRunner.now() - startTime;
    }

    PerfTestRunner.measureValueAsync(totalTime / numberOfTests);
    PerfTestRunner.addRunTestEndMarker();

    if (!isDone) {
      runTest();
    }
  };

  return {
    done: () => {
      isDone = true;
    },
    run: runTest,
  };
}
