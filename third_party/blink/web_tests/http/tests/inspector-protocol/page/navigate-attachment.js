(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.navigate returns isDownload when server responds with Content-Disposition: attachment.`);

  await dp.Page.enable();
  const response = await dp.Page.navigate({ url: testRunner.url('./resources/httpAttachment.php')});
  testRunner.log(response);
  testRunner.completeTest();
})
