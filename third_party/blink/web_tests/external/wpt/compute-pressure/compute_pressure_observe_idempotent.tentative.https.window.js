'use strict';

promise_test(async t => {
  const update = await new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
    observer.observe('cpu').catch(reject);
    observer.observe('cpu').catch(reject);
  });

  assert_equals(typeof update.state, 'string');
  assert_in_array(
      update.state, ['nominal', 'fair', 'serious', 'critical'],
      'cpu pressure state');
}, 'PressureObserver.observe() is idempotent');
