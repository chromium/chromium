(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // TODO(crbug.com/1446498) This test can be deleted once Mutation Events are removed.
  var {page, session, dp} = await testRunner.startBlank('Tests that dom modification event listener produces a violation.');
  dp.Log.onEntryAdded(testRunner.log.bind(testRunner));
  dp.Log.enable();
  dp.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  await session.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);
  testRunner.completeTest();
})
