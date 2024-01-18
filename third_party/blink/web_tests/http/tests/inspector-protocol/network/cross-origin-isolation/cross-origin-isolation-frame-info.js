(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const baseUrl =
      `https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php`;
  const url = `${baseUrl}?coep&corp=same-site&coop`;
  var {page, session, dp} = await testRunner.startURL(
      url,
      'Verifies that we can successfully retrieve frame info from frame tree.');

  await dp.Page.enable();
  const response = await dp.Page.getResourceTree();
  testRunner.log(
      response.result.frameTree.frame, `Frame: `,
      [`adFrameStatus`, `id`, `loaderId`]);
  testRunner.completeTest();
});
