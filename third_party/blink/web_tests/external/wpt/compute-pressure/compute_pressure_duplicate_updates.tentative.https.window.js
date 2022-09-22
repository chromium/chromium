// META: script=/resources/test-only-api.js
// META: script=resources/pressure-helpers.js

'use strict';

pressure_test(async (t, mockPressureService) => {
  const pressureChanges = await new Promise(async resolve => {
    const observer_changes = [];
    let n = 0;
    const observer = new PressureObserver(changes => {
      observer_changes.push(changes);
      if (++n === 2)
        resolve(observer_changes);
    }, {sampleRate: 1.0});
    await observer.observe('cpu');

    mockPressureService.setPressureUpdate('critical');
    mockPressureService.sendUpdate();
    mockPressureService.setPressureUpdate('critical');
    mockPressureService.sendUpdate();
    mockPressureService.setPressureUpdate('nominal');
    mockPressureService.sendUpdate();
  });
  assert_equals(pressureChanges.length, 2);
  assert_equals(pressureChanges[0][0].state, 'critical');
  assert_equals(pressureChanges[1][0].state, 'nominal');
}, 'Changes that fail the "has changes in data" test are discarded.');
