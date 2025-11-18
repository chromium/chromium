/** Get the adapter for the test. If there is none, skip the test and leave the promise unresolved. */
function getAdapter() {
  return new Promise(async resolve => {
    // Always run the test with compat restrictions...
    const adapter = await navigator.gpu?.requestAdapter({ featureLevel: 'compatibility' });
    if (!adapter) {
      skipTest('WebGPU not supported');
      return;
    }
    const hasCore = adapter.features.has('core-features-and-limits');
    PerfTestRunner.log('adapter vendor: ' + adapter.info.vendor);
    PerfTestRunner.log('adapter isFallbackAdapter: ' + adapter.info.isFallbackAdapter);
    PerfTestRunner.log('adapter hasCore: ' + hasCore);

    if (adapter.info.isFallbackAdapter) {
      skipTest('Refusing to run perf test on a fallback (software) adapter');
      return;
    }

    // ... But require the actual adapter to be Core-capable or Core-incapable,
    // depending on the config.
    const compatonly = new URLSearchParams(location.search).has('compatonly');
    if (!compatonly && !hasCore) {
      skipTest('Refusing to run Core perf test on a Core-incapable adapter');
      return;
    }
    if (compatonly && hasCore) {
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
