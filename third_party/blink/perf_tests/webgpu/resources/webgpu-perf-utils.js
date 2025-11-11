/** Get the adapter for the test. If there is none, skip the test and leave the promise unresolved. */
function getAdapter() {
  return new Promise(async resolve => {
    const adapter = await navigator.gpu?.requestAdapter({ featureLevel: 'compatibility' });
    if (!adapter) {
      skipTest('WebGPU not supported');
      return;
    }
    const hasCore = adapter.features.has('core-features-and-limits');
    PerfTestRunner.log('adapter vendor: ' + adapter.info.vendor);
    PerfTestRunner.log('adapter hasCore: ' + hasCore);
    if (new URLSearchParams(location.search).has('compatonly') && hasCore) {
      skipTest('Refusing to run Compat perf test on a Core-capable adapter');
      return;
    }
    resolve(adapter);
  });
}

function skipTest(message) {
  PerfTestRunner.log('FATAL: ' + message);

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
