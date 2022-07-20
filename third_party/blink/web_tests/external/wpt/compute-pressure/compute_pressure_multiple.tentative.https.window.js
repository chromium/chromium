'use strict';

promise_test(async t => {
  const update1_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(
        resolve, {cpuUtilizationThresholds: [0.5]});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const update2_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(
        resolve, {cpuUtilizationThresholds: [0.5]});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const update3_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(
        resolve, {cpuUtilizationThresholds: [0.5]});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const [update1, update2, update3] =
      await Promise.all([update1_promise, update2_promise, update3_promise]);

  for (const update of [update1, update2, update3]) {
    assert_in_array(update.cpuUtilization, [0.25, 0.75],
                    'cpuUtilization quantization');
  }
}, 'Three PressureObserver instances with the same quantization ' +
   'schema receive updates');
