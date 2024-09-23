(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that permissions can be changed`);

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  await session.evaluateAsync(async () => {
    window.messages = [];
  });

  // Test that all three states can be set.
  await Promise.all([
    setWithName('background-fetch', 'granted'),
    setWithName('persistent-storage', 'prompt'),
    setWithName('background-sync', 'denied')
  ]);
  await Promise.all([
    waitForName('background-fetch', 'granted'),
    waitForName('persistent-storage', 'prompt'),
    waitForName('background-sync', 'denied')
  ]);
  await dp.Browser.resetPermissions();

  // Test bad state.
  await Promise.all([
    setWithName('background-fetch', 'foo')
  ]);
  await dp.Browser.resetPermissions();

  // Test states can be changed for single permission.
  await setWithName('background-fetch', 'granted');
  await waitForName('background-fetch', 'granted');
  await setWithName('background-fetch', 'denied');
  await waitForName('background-fetch', 'denied');
  await setWithName('background-fetch', 'prompt');
  await waitForName('background-fetch', 'prompt');
  await dp.Browser.resetPermissions();

  // Test MIDI rules are respected.
  const midi_with_sysex = {name: 'midi', sysex: true};
  const midi_without_sysex = {name: 'midi', sysex: false};

  // Granting sysex=true implies granting sysex=false.
  await set(midi_with_sysex, 'granted');
  await Promise.all([
    waitPermission(midi_with_sysex, 'granted'),
    waitPermission(midi_without_sysex, 'granted')
  ]);

  // Denying sysex=false implies denying sysex=true.
  await set(midi_without_sysex, 'denied');
  await Promise.all([
    waitPermission(midi_without_sysex, 'denied'),
    waitPermission(midi_with_sysex, 'denied')
  ]);

  // Prompt sysex=false implies prompt sysex=true.
  await set(midi_without_sysex, 'prompt');
  await Promise.all([
    waitPermission(midi_without_sysex, 'prompt'),
    waitPermission(midi_with_sysex, 'prompt')
  ]);
  await dp.Browser.resetPermissions();

  // Test "push" permissions userVisibleOnly=true is supported.
  await set({name: 'push', userVisibleOnly: true}, 'granted');
  await waitPermission({name: 'push', userVisibleOnly: true}, 'granted');
  await dp.Browser.resetPermissions();

  // Test "camera" permission panTiltZoom=true is supported.
  await set({name: 'camera', panTiltZoom: true}, 'granted');
  await waitPermission({name: 'camera', panTiltZoom: true}, 'granted');
  await dp.Browser.resetPermissions();

  // Test "fullscreen" permission allowWithoutGesture=true is supported.
  await set({name: 'fullscreen', allowWithoutGesture: true}, 'granted');
  await waitPermission({name: 'fullscreen', allowWithoutGesture: true}, 'granted');
  await dp.Browser.resetPermissions();

  // Test unsupported "fullscreen" permission descriptor options.
  await set({name: 'fullscreen', allowWithoutGesture: false}, 'granted');
  await set({name: 'fullscreen'}, 'granted');
  await dp.Browser.resetPermissions();

  // Cross-origin test.
  await setWithName('geolocation', 'granted');
  await set({name: 'geolocation'}, 'denied', 'http://devtools.txt:8001');
  await waitForName('geolocation', 'granted');
  await dp.Browser.resetPermissions()

  testRunner.log(await session.evaluate(() => window.messages));

  testRunner.completeTest();

  async function set(descriptor, state, url) {
    const response = await dp.Browser.setPermission({
      origin: (url === undefined) ? 'http://devtools.test:8000' : url,
      permission: descriptor,
      setting: state
    });
    if (response.error)
      testRunner.log('- Failed to set: ' + JSON.stringify(descriptor) + '  error: ' + response.error.message);
    else
      testRunner.log('- Set: ' + JSON.stringify(descriptor) + ' to ' + state);
  }

  async function waitPermission(descriptor, state) {
    await session.evaluateAsync(async (descriptor, state) => {
      const result = await navigator.permissions.query(descriptor);
      if (result.state && result.state === state)
        window.messages.push(`${JSON.stringify(descriptor)}: ${result.state}`);
      else
        window.messages.push(`Failed to set ${JSON.stringify(descriptor)} to state: ${state}. Got ${result.state}`);
    }, descriptor, state);
  }

  async function waitForName(name, state) {
    await waitPermission({'name': name}, state);
  }

  async function setWithName(name, state) {
    await set({'name': name}, state);
  }

})

