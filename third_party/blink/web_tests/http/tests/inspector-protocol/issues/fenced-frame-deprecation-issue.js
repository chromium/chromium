(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that fenced frame deprecation issue is reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded(
      event => event.params.issue.code === 'DeprecationIssue' &&
          event.params.issue.details?.deprecationIssueDetails?.type ===
              'FencedFrame');
  await session.evaluate(`
    const frame = document.createElement('fencedframe');
    document.body.appendChild(frame);
  `);
  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})
