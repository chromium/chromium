const publicKeyConfig = `{
  'originScopedKeys': {
    'https://a.test:8443': {
      'keys':[{
        'id':'14345678-9abc-def0-1234-56789abcdef0',
        'key':'oV9AZYb6xHuZWXDxhdnYkcdNzx65Gn1QpYsBaD5gBS0='}]
    }
  }
}`;

const baseOrigin = 'https://a.test:8443/';
const base = baseOrigin + 'inspector-protocol/resources/';
const urn = 'urn:uuid://123e4567-e89b-12d3-a456-426614174000';

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that deprecation issues are reported for protected audience.\n`);

  await dp.Audits.enable();

  const coordinator = 'https://padeprecation.test';
  await dp.Browser.addPrivacySandboxCoordinatorKeyConfig({
    api: 'BiddingAndAuctionServices',
    coordinatorOrigin: coordinator,
    keyConfig: publicKeyConfig
  });

  async function evaluateAndLogIssue(js) {
    // Unmute deprecation issue reporting by navigating (only gives one issue
    // per page)
    await page.navigate(base + 'empty.html');
    let promise = dp.Audits.onceIssueAdded();
    await session.evaluateAsync(js);
    let result = await promise;
    testRunner.log(result.params, 'Inspector issue: ');
  }

  await evaluateAndLogIssue(
      `navigator.joinAdInterestGroup({name: 1, owner: '${baseOrigin}'}, 3000)`);
  await evaluateAndLogIssue(
      `navigator.leaveAdInterestGroup({name: 1, owner: '${baseOrigin}'})`);
  await evaluateAndLogIssue(
      `navigator.clearOriginJoinedAdInterestGroups('${baseOrigin}')`);
  await evaluateAndLogIssue('navigator.updateAdInterestGroups()');
  await evaluateAndLogIssue('navigator.createAuctionNonce()');
  await evaluateAndLogIssue(`navigator.runAdAuction({
    decisionLogicURL: '${base + 'fledge_decision_logic.js.php'}',
    seller: '${baseOrigin}',
  })`);
  await evaluateAndLogIssue('navigator.adAuctionComponents(0)');
  await evaluateAndLogIssue(`navigator.deprecatedURNToURL('${urn}')`);
  await evaluateAndLogIssue(`navigator.deprecatedReplaceInURN('${urn}', {})`);
  await evaluateAndLogIssue(
      `navigator.getInterestGroupAdAuctionData({seller: '${
          baseOrigin}', coordinatorOrigin: '${coordinator}'})`);
  await evaluateAndLogIssue('navigator.canLoadAdAuctionFencedFrame()');
  await evaluateAndLogIssue(
      'navigator.deprecatedRunAdAuctionEnforcesKAnonymity');
  await evaluateAndLogIssue('navigator.protectedAudience');
  testRunner.completeTest();
})
