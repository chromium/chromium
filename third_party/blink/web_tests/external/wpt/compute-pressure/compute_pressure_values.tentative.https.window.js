'use strict';

promise_test(async t => {
  // The quantization thresholds and the quantized values that they lead to can
  // be represented exactly in floating-point, so === comparison works.

  const update = await new Promise((resolve, reject) => {
    const observer = new PressureObserver(
        resolve,
        {cpuUtilizationThresholds: [0.25]});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  assert_in_array(
      update.cpuUtilization, [0.125, 0.625], 'cpuUtilization quantization');
}, 'PressureObserver quantizes utilization and speed separately');
