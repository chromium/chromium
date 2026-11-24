// Queues two animation frame callbacks synchronously so that they run in the
// same frame, and reports whether a microtask queued by the first callback ran
// before the second one was invoked. The reply itself is sent from a microtask
// queued by the last callback, so it only arrives if that microtask runs too.
self.onmessage = () => {
  let microtask_ran = false;
  requestAnimationFrame(() => {
    Promise.resolve().then(() => {
      microtask_ran = true;
    });
  });
  requestAnimationFrame(() => {
    const ran_between_callbacks = microtask_ran;
    Promise.resolve().then(() => {
      postMessage(ran_between_callbacks);
    });
  });
};
