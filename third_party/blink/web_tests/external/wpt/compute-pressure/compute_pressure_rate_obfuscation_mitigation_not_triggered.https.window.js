// META: timeout=long
// META: variant=?globalScope=window
// META: variant=?globalScope=dedicated_worker
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=./resources/common.js

'use strict';

pressure_test(async (t) => {
  await create_virtual_pressure_source('cpu');
  t.add_cleanup(async () => {
    await remove_virtual_pressure_source('cpu');
  });

  const sampleIntervalInMs = 100;
  const readings = ['nominal', 'fair', 'serious', 'critical'];
  // Normative values for rate obfuscation parameters.
  // https://w3c.github.io/compute-pressure/#rate-obfuscation-normative-parameters.
  const minPenaltyTimeInMs = 5000;
  const minChangesThreshold = 50;

  const changes = [];
  const observer = new PressureObserver(updates => {
    changes.push(updates);
  });
  t.add_cleanup(() => observer.disconnect());
  await observer.observe('cpu', {sampleInterval: sampleIntervalInMs});

  let i = 0;
  while (changes.length < minChangesThreshold) {
    await update_virtual_pressure_source(
        'cpu', readings[i++ % readings.length]);
    await t.step_wait(
        () => changes.length >= i,
        `At least ${i} readings have been delivered`);
  }
  observer.disconnect();

  assert_equals(changes.length, minChangesThreshold);

  for (let i = 0; i < (changes.length - 1); i++) {
    // Because no penalty should be triggered, the timestamp difference
    // between samples should be less than the minimum penalty.
    assert_less_than(
        changes[i + 1][0].time - changes[i][0].time, minPenaltyTimeInMs,
        'Not in sample time boundaries');
  }
}, 'No rate obfuscation mitigation should happen, when number of changes is below minimum changes before penalty');

mark_as_done();
