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

  const sampleIntervalInMs = 40;
  const readings = ['nominal', 'fair', 'serious', 'critical'];
  // Normative values for rate obfuscation parameters.
  // https://w3c.github.io/compute-pressure/#rate-obfuscation-normative-parameters.
  const minPenaltyTimeInMs = 5000;
  const maxChangesThreshold = 100;
  const minChangesThreshold = 50;

  let gotPenalty = false;

  const observerChanges = [];
  const observer = new PressureObserver(changes => {
    if (observerChanges.length >= (minChangesThreshold - 1)) {
      const lastSample = observerChanges.at(-1);
      if ((changes[0].time - lastSample[0].time) >= minPenaltyTimeInMs) {
        // The update delivery might still be working even if
        // maxChangesThreshold have been reached and before disconnect() is
        // processed.
        // Therefore we are adding a flag to dismiss any updates after the
        // penalty is detected, which is the condition for the test to pass.
        gotPenalty = true;
        observer.disconnect();
      }
    }
    observerChanges.push(changes);
  });
  t.add_cleanup(() => observer.disconnect());
  observer.observe('cpu', {sampleInterval: sampleIntervalInMs});

  let i = 0;
  while (observerChanges.length <= maxChangesThreshold && !gotPenalty) {
    const size = observerChanges.length;
    await update_virtual_pressure_source(
        'cpu', readings[i++ % readings.length]);
    await t.step_wait(
        () => observerChanges.length >= i, `${i} readings have been delivered`,
        minPenaltyTimeInMs * 10);
  }

  assert_true(gotPenalty, 'Penalty triggered');
}, 'Rate obfuscation mitigation should have been triggered, when changes is higher than minimum changes before penalty');

mark_as_done();
