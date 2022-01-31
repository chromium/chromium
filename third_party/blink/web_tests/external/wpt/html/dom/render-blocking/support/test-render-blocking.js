class LoadObserver {
  constructor(object) {
    this.finishTime = null;
    this.load = new Promise((resolve, reject) => {
      object.onload = ev => {
        this.finishTime = ev.timeStamp;
        resolve(ev);
      };
      object.onerror = reject;
    });
  }

  get finished() {
    return this.finishTime !== null;
  }
}

// Error margin for comparing timestamps of paint and load events, in case they
// are reported by different threads.
const epsilon = 50;

function test_render_blocking(finalTest, finalTestTitle) {
  // Ideally, we should observe the 'load' event on the specific render-blocking
  // elements. However, this is not possible for script-blocking stylesheets, so
  // we have to observe the 'load' event on 'window' instead.
  // TODO(xiaochengh): Add tests for other types of render-blocking elements and
  // observe the specific 'load' events on them.
  const loadObserver = new LoadObserver(window);

  promise_test(async test => {
    assert_implements(window.PerformancePaintTiming);

    await test.step_wait(() => performance.getEntriesByType('paint').length);

    assert_true(loadObserver.finished);
    for (let entry of performance.getEntriesByType('paint')) {
      assert_greater_than(entry.startTime, loadObserver.finishTime - epsilon,
                          `${entry.name} should occur after loading render-blocking resources`);
    }
  }, 'Rendering is blocked before render-blocking resources are loaded');

  promise_test(test => {
    return loadObserver.load.then(() => finalTest(test));
  }, finalTestTitle);
}

