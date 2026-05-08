(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verify that BackUINavigationWouldSkipAd generic issue is generated when Back-To-Ad Intervention would trigger.`);

  await dp.Audits.enable();

  // Navigate to a cross-origin page first to ensure a cross-origin skip happens
  // later. We use devtools.test:8443 as the starting origin.
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html');

  // Navigate to another origin where the ad script will run.
  // a.test:8443 is cross-origin to devtools.test:8443.
  await page.navigate(
      'https://a.test:8443/inspector-protocol/resources/back-to-ad-skip.html');

  // Trigger user activation. This marks the page unskippable for the original
  // history manipulation intervention, ensuring the back-to-ad intervention is
  // effective. Otherwise, it would be superseded by the original history
  // intervention.
  await dp.Input.dispatchMouseEvent(
      {type: 'mousePressed', button: 'left', x: 0, y: 0, clickCount: 1});
  await dp.Input.dispatchMouseEvent(
      {type: 'mouseReleased', button: 'left', x: 0, y: 0, clickCount: 1});

  // Call the ad script function and wait for it to finish.
  await session.evaluateAsync(`insertAdEntry()`);

  const issuePromise = dp.Audits.onceIssueAdded(issue => {
    return issue.params.issue.details.genericIssueDetails &&
        issue.params.issue.details.genericIssueDetails.errorType ===
        'BackUINavigationWouldSkipAd';
  });

  // Trigger another click to make the browser check the back button state
  // again. This should trigger GetIndexWithSkipping and the intervention logic.
  await dp.Input.dispatchMouseEvent(
      {type: 'mousePressed', button: 'left', x: 0, y: 0, clickCount: 1});
  await dp.Input.dispatchMouseEvent(
      {type: 'mouseReleased', button: 'left', x: 0, y: 0, clickCount: 1});

  const issue = await issuePromise;
  testRunner.log(
      issue.params.issue.details.genericIssueDetails,
      'Generic Issue Details: ');

  testRunner.completeTest();
})
