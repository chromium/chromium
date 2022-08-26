// META: script=/resources/test-only-api.js
// META: script=resources/pressure-helpers.js

'use strict';

pressure_test((t, mockPressureService) => {
  const observer = new PressureObserver(() => {
    assert_unreached('The observer callback should not be called');
  });

  mockPressureService.setExpectedFailure(
      new DOMException('', 'NotSupportedError'));
  return promise_rejects_dom(t, 'NotSupportedError', observer.observe('cpu'));
}, 'Return NotSupportedError when calling observer()');

pressure_test((t, mockPressureService) => {
  const observer = new PressureObserver(() => {
    assert_unreached('The observer callback should not be called');
  });

  mockPressureService.setExpectedFailure(new DOMException('', 'SecurityError'));
  return promise_rejects_dom(t, 'SecurityError', observer.observe('cpu'));
}, 'Return SecurityError when calling observer()');

pressure_test(async (t, mockPressureService) => {
  const update = await new Promise(async resolve => {
    const observer = new PressureObserver(resolve);
    await observer.observe('cpu');

    mockPressureService.setPressureState({cpuUtilization: 0.5});
    mockPressureService.sendUpdate();
  });
  assert_equals(update.cpuUtilization, 0.5);
}, 'Basic functionality test');

pressure_test((t, mockPressureService) => {
  const observer = new PressureObserver(() => {
    assert_unreached('The observer callback should not be called');
  });

  observer.observe('cpu');
  observer.unobserve('cpu');
  mockPressureService.setPressureState({cpuUtilization: 0.5});
  mockPressureService.sendUpdate();

  return new Promise(resolve => t.step_timeout(resolve, 1000));
}, 'Removing observer before observe() resolves works');

pressure_test(async (t, mockPressureService) => {
  const callbackPromises = [];
  const observePromises = [];

  for (let i = 0; i < 2; i++) {
    callbackPromises.push(new Promise(resolve => {
      const observer = new PressureObserver(resolve);
      observePromises.push(observer.observe('cpu'));
    }));
  }

  await Promise.all(observePromises);

  mockPressureService.setPressureState({cpuUtilization: 0.5});
  mockPressureService.sendUpdate();

  return Promise.all(callbackPromises);
}, 'Calling observe() multiple times works');

pressure_test(async (t, mockPressureService) => {
  const update = await new Promise(async resolve => {
    const observer1 =
        new PressureObserver(resolve, {cpuUtilizationThresholds: [0.5]});
    await observer1.observe('cpu');

    const observer2 =
        new PressureObserver(() => {}, {cpuUtilizationThresholds: [0.5]});
    await observer2.observe('cpu');

    mockPressureService.setPressureState({cpuUtilization: 0.5});
    mockPressureService.sendUpdate();
  });

  assert_equals(update.cpuUtilization, 0.5);
}, 'Same quantization should not stop other observers');

pressure_test(async (t, mockPressureService) => {
  const observer1 = new PressureObserver(() => {
    assert_unreached('The observer callback should not be called');
  }, {cpuUtilizationThresholds: [0.5]});
  await observer1.observe('cpu');

  const observer2 =
      new PressureObserver(() => {}, {cpuUtilizationThresholds: [0.3]});
  await observer2.observe('cpu');

  mockPressureService.setPressureState({cpuUtilization: 0.5});
  mockPressureService.sendUpdate();

  await new Promise(resolve => t.step_timeout(resolve, 1000));
}, 'Different quantization should stop other observers');
