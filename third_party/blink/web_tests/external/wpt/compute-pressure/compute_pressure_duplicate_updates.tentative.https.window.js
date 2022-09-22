// META: script=/resources/test-only-api.js
// META: script=resources/pressure-helpers.js

'use strict';

pressure_test(async (t, mockPressureService) => {
  const pressureUpdates = await new Promise(async resolve => {
    const updates = [];
    let n = 0;
    const observer = new PressureObserver(update => {
      updates.push(update);
      if (++n === 2)
        resolve(updates);
    }, {sampleRate: 1.0});
    await observer.observe('cpu');

    mockPressureService.setPressureUpdate('critical');
    mockPressureService.sendUpdate();
    mockPressureService.setPressureUpdate('critical');
    mockPressureService.sendUpdate();
    mockPressureService.setPressureUpdate('nominal');
    mockPressureService.sendUpdate();
  });
  assert_equals(pressureUpdates.length, 2);
  assert_equals(pressureUpdates[0][0].state, 'critical');
  assert_equals(pressureUpdates[1][0].state, 'nominal');
}, 'Updates that fail the "has change in data" test are discarded.');
