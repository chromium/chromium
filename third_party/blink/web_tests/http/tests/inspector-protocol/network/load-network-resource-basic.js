(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests for Network.loadNetworkResource on the same origin`);

  await dp.Network.enable();

  const frameId = (await dp.Target.getTargetInfo()).result.targetInfo.targetId;

  async function requestSourceMap(frameId, testExplanation, url) {
    const response = await dp.Network.loadNetworkResource({frameId, url, options: {disableCache:false, includeCredentials: false}});
    testRunner.log(response.result, testExplanation, ["headers", "stream"]);
    if (response.result.resource.success) {
      let result = await dp.IO.read({handle: response.result.resource.stream, size: 1000*1000});
      testRunner.log(result);
      await dp.IO.close({handle: response.result.resource.stream});
    }
  }

  const urlWithMimeType = `http://localhost:8000/inspector-protocol/network/resources/source.map.php`;
  await requestSourceMap(frameId, `Response for fetch with existing resource with text content type: `, urlWithMimeType);

  const urlWithoutMimeType = `http://localhost:8000/inspector-protocol/network/resources/source.map`;
  await requestSourceMap(frameId, `Response for fetch with existing resource without content type: `, urlWithoutMimeType);

  const nonExistentUrl = `http://localhost:8000/inspector-protocol/network/resources/source.map-DOES-NOT-EXIST`;
  await requestSourceMap(frameId, `Response for fetch with non-existing resource: `, nonExistentUrl);

  testRunner.completeTest();
})
