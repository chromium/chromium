'use strict';

promise_test(async t => {
  const observer1_updates = [];
  const observer1 = new PressureObserver(update => {
    observer1_updates.push(update);
  }, {sampleRate: 1});
  t.add_cleanup(() => observer1.disconnect());
  // Ensure that observer1's schema gets registered before observer2 starts.
  observer1.observe('cpu');
  observer1.disconnect();

  const observer2_updates = [];
  await new Promise((resolve, reject) => {
    const observer2 = new PressureObserver(update => {
      observer2_updates.push(update);
      resolve();
    }, {sampleRate: 1});
    t.add_cleanup(() => observer2.disconnect());
    observer2.observe('cpu').catch(reject);
  });

  assert_equals(
      observer1_updates.length, 0,
      'stopped observers should not receive callbacks');

  assert_equals(observer2_updates.length, 1);
  assert_in_array(
      observer2_updates[0].state, ['nominal', 'fair', 'serious', 'critical'],
      'cpu pressure state');
}, 'Stopped PressureObserver do not receive updates');
