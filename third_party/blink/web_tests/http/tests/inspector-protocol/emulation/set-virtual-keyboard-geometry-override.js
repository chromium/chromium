(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests virtual keyboard geometry emulation, validation, and clearing.');

  await dp.Emulation.setDeviceMetricsOverride({
    width: 400,
    height: 800,
    deviceScaleFactor: 1,
    mobile: false,
  });

  async function installEventCounter() {
    await session.evaluate(`
      window.geometryChangeCount = 0;
      window.geometryAtEvent = null;
      navigator.virtualKeyboard.addEventListener('geometrychange', () => {
        ++window.geometryChangeCount;
        window.geometryAtEvent = navigator.virtualKeyboard.boundingRect.height;
      });
    `);
  }

  async function readGeometry() {
    return await session.evaluate(`(() => {
      const env = name => {
        const element = document.createElement('div');
        element.style.width = 'env(' + name + ')';
        document.body.appendChild(element);
        const value = element.getBoundingClientRect().width;
        element.remove();
        return value;
      };
      const rect = navigator.virtualKeyboard.boundingRect;
      const insetNames = ['top', 'left', 'bottom', 'right', 'width', 'height'];
      const atEvent = window.geometryAtEvent;
      return [
        'rect=' + [rect.x, rect.y, rect.width, rect.height].join(','),
        'insets=' + insetNames.map(name => env('keyboard-inset-' + name)).join(','),
        'events=' + window.geometryChangeCount,
        'event=' + (atEvent ?? 'null'),
      ].join(' ');
    })()`);
  }

  async function readApiGeometry() {
    return await session.evaluate(`(() => {
      const rect = navigator.virtualKeyboard.boundingRect;
      return [
        'rect=' + [rect.x, rect.y, rect.width, rect.height].join(','),
        'events=' + window.geometryChangeCount,
        'event=' + (window.geometryAtEvent ?? 'null'),
      ].join(' ');
    })()`);
  }

  const keyboardRect = {x: 10.4, y: 500.4, width: 379.6, height: 249.6};
  await installEventCounter();
  await dp.Emulation.setVirtualKeyboardGeometryOverride({keyboardRect});
  testRunner.log(`After setting geometry: ${await readApiGeometry()}`);

  await dp.Emulation.setVirtualKeyboardGeometryOverride({keyboardRect});
  const eventCount = await session.evaluate('window.geometryChangeCount');
  testRunner.log(`Event count after setting identical geometry: ${eventCount}`);

  const invalid = await dp.Emulation.setVirtualKeyboardGeometryOverride(
      {keyboardRect: {x: 0, y: 0, width: -1, height: 100}});
  testRunner.log(`Invalid geometry error: ${invalid.error?.message}`);
  testRunner.log(`After rejected geometry: ${await readGeometry()}`);

  await dp.Emulation.setVirtualKeyboardGeometryOverride({});
  testRunner.log(`After clearing geometry: ${await readGeometry()}`);

  await dp.Emulation.setVirtualKeyboardGeometryOverride({keyboardRect});
  await session.disconnect();
  const nextSession = await page.createSession();
  const clearedRect = await nextSession.evaluate(`(() => {
    const rect = navigator.virtualKeyboard.boundingRect;
    return [rect.x, rect.y, rect.width, rect.height].join(',');
  })()`);
  testRunner.log(`After session disconnect: rect=${clearedRect}`);

  testRunner.completeTest();
});
