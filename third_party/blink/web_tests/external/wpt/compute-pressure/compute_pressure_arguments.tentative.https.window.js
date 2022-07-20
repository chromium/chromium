'use strict';

for (const property of ['cpuUtilizationThresholds']) {
  for (const out_of_range_value of [-1.0, 0.0, 1.0, 2.0]) {
    test(t => {
      const callback = () => {};

      const options = {
          cpuUtilizationThresholds: [0.5] };
      options[property] = [out_of_range_value];

      assert_throws_js(TypeError, () => {
        new PressureObserver(callback, options);
      });
    }, `PressureObserver constructor throws when ${property} ` +
       `is [${out_of_range_value}]`);
  }

  for (const valid_value of [0.05, 0.1, 0.2, 0.5, 0.9, 0.95]) {
    test(t => {
      const callback = () => {};

      const options = {
          cpuUtilizationThresholds: [0.5] };
      options[property] = [valid_value];

      const observer = new PressureObserver(callback, options);
      assert_true(observer instanceof PressureObserver);
    }, `PressureObserver constructor accepts ${property} value ` +
       `[${valid_value}]`);
  }
}

test(t => {
  const callback = () => {};


  assert_throws_js(TypeError, () => {
    new PressureObserver(
        callback,
        { cpuUtilizationThresholds: [0.5, 0.5] });
  });
}, 'PressureObserver constructor throws when cpuUtilizationThresholds ' +
   'has duplicates');
