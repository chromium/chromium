'use strict';

promise_test(async t => {
  const update1_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const update2_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const update3_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const [update1, update2, update3] =
      await Promise.all([update1_promise, update2_promise, update3_promise]);

  for (const update of [update1, update2, update3]) {
    assert_in_array(
        update.state, ['nominal', 'fair', 'serious', 'critical'],
        'cpu pressure state');
  }
}, 'Three PressureObserver instances receive updates');
