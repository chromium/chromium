(async function(testRunner) {
  const worker = `console.log("Worker");
  onmessage = function(e) {
    console.log(e);
  };`;
  const baseUrl =
      `https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php`;
  const url = `${baseUrl}?coep&corp=same-site&coop`;
  var {page, session, dp} = await testRunner.startURL(
      url,
      'Verifies that we can successfully retrieve the security isolation status of a dedicated worker.');

  await dp.Page.enable();
  const response = await dp.Page.getResourceTree();
  testRunner.log(response.result);
  testRunner.completeTest();
});
