'use strict';

test(t => {
  const observer = new PressureObserver(
      t.unreached_func('This callback should not have been called.'),
      {cpuUtilizationThresholds: [0.25]});

  const records = observer.takeRecords();
  assert_equals(records.length, 0, 'No record before observe');
}, 'Calling takeRecords() before observe()');

promise_test(async t => {
  let observer;
  const record = await new Promise((resolve, reject) => {
    observer = new PressureObserver(
        resolve,
        {cpuUtilizationThresholds: [0.25]});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  assert_in_array(
      record.cpuUtilization, [0.125, 0.625], 'cpuUtilization quantization');

  const records = observer.takeRecords();
  assert_equals(records.length, 0, 'No record available');
}, 'takeRecords() returns empty record after callback invoke');
