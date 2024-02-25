(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests basic error validation for Network.loadNetworkResource`);

  await dp.Network.enable();

  const response1 = await dp.Network.loadNetworkResource({
    frameId: `invalid`,
    url: `invalid`,
    options: {disableCache: false, includeCredentials: false}
  });
  testRunner.log(response1, `Response for invalid target and invalid url: `);

  const response2 = await dp.Network.loadNetworkResource({
    frameId: `invalid`,
    url: `https://example.com/source.map`,
    options: {disableCache: false, includeCredentials: false}
  });
  testRunner.log(response2, `Response for invalid target and valid url: `);

  const response3 = await dp.Network.loadNetworkResource({
    url: `https://example.com/source.map`,
    options: {disableCache: false, includeCredentials: false}
  });
  testRunner.log(response3, `Response for omitted frameId: `);

  const frameId = (await dp.Target.getTargetInfo()).result.targetInfo.targetId;

  const response4 = await dp.Network.loadNetworkResource({
    frameId,
    url: `invalid`,
    options: {disableCache: false, includeCredentials: false}
  });
  testRunner.log(response4, `Response for valid target and invalid url: `);

  testRunner.completeTest();
})
