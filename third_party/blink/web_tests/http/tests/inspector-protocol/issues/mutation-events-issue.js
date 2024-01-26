(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // TODO(crbug.com/1446498) This test can be deleted once Mutation Events are removed.
  const { session, dp } = await testRunner.startBlank(
  'Verifies that adding Mutation Event listeners triggers a deprecation issue.');
  await dp.Audits.enable();

  const events = [
    'DOMCharacterDataModified',
    'DOMNodeInserted',
    'DOMNodeInsertedIntoDocument',
    'DOMNodeRemoved',
    'DOMNodeRemovedFromDocument',
    'DOMSubtreeModified',
  ];

  for (const evt of events) {
    const promise = dp.Audits.onceIssueAdded();
    await session.navigate(`../resources/mutation-events.html?${evt}`);
    const result = await promise;
    testRunner.log(result.params, `Issue for ${evt}: `);
  }

  // Now make sure a non-mutation event doesn't trigger the warning
  const promise = dp.Audits.onceIssueAdded();
  await session.navigate(`../resources/mutation-events.html?load`);
  let noIssue = false;
  const wait2frames = new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(() => {noIssue=true; resolve();})));
  await Promise.any([wait2frames,promise]);
  testRunner.log(`Non-Mutation Event generated issue: ${noIssue ? "PASS" : "FAIL"}`);

  testRunner.completeTest();
})
