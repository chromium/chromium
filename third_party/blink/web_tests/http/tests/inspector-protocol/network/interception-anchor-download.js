(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that downloads are intercepted when interception is enabled for page.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});

  session.evaluate(`
    const a = document.createElement('a');
    a.href = '/devtools/network/resources/resource.php';
    a.download = 'hello.text';
    document.body.appendChild(a);
    a.click();
  `);

  const event = await dp.Network.onceRequestIntercepted();
  testRunner.log(`Intercepted: ${event.params.request.url}`);

  testRunner.completeTest();
})

