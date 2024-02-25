(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
  'Verifies that ObsoleteCreateImageBitmapImageOrientationNone deprecation issue is created ' +
  'from page with {imageOrientation: "none"}.');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();

  await session.navigate('../resources/create-image-bitmap-none.html');

  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})
