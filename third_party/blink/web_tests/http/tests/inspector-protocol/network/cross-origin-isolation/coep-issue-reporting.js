(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that cross-origin embedder policy (COEP) related blocking is reported correctly.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const expectedNumberOfIssues = 8;
  const issues = [];

  function record(issue) {
    issues.push(issue);

    if (issues.length === expectedNumberOfIssues) {
      function compareIssue(a, b) {
        return `${a.details?.blockedByResponseIssueDetails?.request?.url}`.localeCompare(`${b.details?.blockedByResponseIssueDetails?.request?.url}`);
      }
      issues.sort(compareIssue);
      for (const entry of issues) {
        testRunner.log(entry, "Issue reported: ", ['requestId', 'frameId']);
      }
      testRunner.completeTest();
    }
  }

  async function initalizeTarget(dp) {
    dp.Audits.onIssueAdded(event => record(event.params.issue)),
    await Promise.all([
      dp.Audits.enable(),
      dp.Page.enable()
    ]);
  }

  await initalizeTarget(dp);

  dp.Target.onAttachedToTarget(async e => {
    const dp = session.createChild(e.params.sessionId).protocol;
    await initalizeTarget(dp);
  });

  page.navigate('https://devtools.test:8443/inspector-protocol/network/cross-origin-isolation/resources/coep-page-with-resources.php');

  // `record` above completes the test if all expected issues were received.
})

