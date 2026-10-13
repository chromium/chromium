(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        ::view-transition-group(root) { animation-duration: 300s; }
        ::view-transition-new(root) { filter: url(#blur); }
      </style>
      <svg width=0 height=0>
        <filter id=blur><feGaussianBlur stdDeviation=5></feGaussianBlur></filter>
      </svg>`,
      'Tests that a deprecation issue is reported when an SVG filter is applied to view transition content');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded(issue => issue.params.issue.code ==
                                               'DeprecationIssue');
  await session.evaluateAsync('document.startViewTransition().ready');
  const result = await promise;
  const issue = result.params.issue.details.deprecationIssueDetails;
  testRunner.log(`Inspector issue type ${issue.type}`);
  testRunner.completeTest();
})
