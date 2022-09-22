'use strict';

promise_test(async t => {
  await new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });
}, 'An active PressureObserver calls its callback at least once');

promise_test(async t => {
  const observer1_updates = [];
  const update = await new Promise(async resolve => {
    const observer1 = new PressureObserver(
        update1 => {observer1_updates.push(update1)}, {sampleRate: 1.0});
    await observer1.observe('cpu');

    const observer2 = new PressureObserver(resolve, {sampleRate: 1.0});
    await observer2.observe('cpu');
  });

  assert_in_array(
      update.state, ['nominal', 'fair', 'serious', 'critical'],
      'cpu pressure state');
  assert_in_array(
      observer1_updates[0].state, ['nominal', 'fair', 'serious', 'critical'],
      'cpu pressure state');
}, 'Newly registered observer should get update');
