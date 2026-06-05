(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that using the deprecated WebTransport datagram high water ` +
      `mark attributes causes issues.\n`);

  await dp.Audits.enable();

  const expectedTypes = [
    'WebTransportDatagramDuplexStreamIncomingHighWaterMark',
    'WebTransportDatagramDuplexStreamOutgoingHighWaterMark',
  ];
  const pendingTypes = new Set(expectedTypes);
  const issuesReported = new Promise(resolve => dp.Audits.onIssueAdded(event => {
    const issue = event.params.issue;
    if (issue.code !== 'DeprecationIssue')
      return;

    const type = issue.details?.deprecationIssueDetails?.type;
    if (!pendingTypes.has(type))
      return;

    pendingTypes.delete(type);
    if (!pendingTypes.size) {
      resolve();
    }
  }));

  await session.evaluate(`
    const transport = new WebTransport('https://localhost/');
    transport.ready.catch(() => {});
    transport.closed.catch(() => {});
    transport.datagrams.incomingHighWaterMark;
    transport.datagrams.outgoingHighWaterMark;
    transport.close();
  `);

  await issuesReported;
  for (const type of expectedTypes)
    testRunner.log(`Inspector issue type: ${type}`);

  testRunner.completeTest();
})
