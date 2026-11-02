(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that lazy-load image zero-size issues are reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`
    // Sized images (Cases A, B, C, D and Report 1/2 responsive image)
    // None of these should trigger a LazyLoadImageIssue.
    const sizedA = document.createElement('img');
    sizedA.src = 'data:image/png;caseA';
    sizedA.loading = 'lazy';
    sizedA.width = 100;
    sizedA.height = 100;
    document.body.appendChild(sizedA);

    const sizedB = document.createElement('img');
    sizedB.src = 'data:image/png;caseB';
    sizedB.loading = 'lazy';
    sizedB.width = 100;
    sizedB.height = 100;
    sizedB.style.height = 'auto';
    document.body.appendChild(sizedB);

    const sizedC = document.createElement('img');
    sizedC.src = 'data:image/png;caseC';
    sizedC.loading = 'lazy';
    sizedC.width = 100;
    sizedC.height = 100;
    sizedC.style.cssText = 'max-width:100%;height:auto';
    document.body.appendChild(sizedC);

    const sizedD = document.createElement('img');
    sizedD.src = 'data:image/png;caseD';
    sizedD.loading = 'lazy';
    sizedD.width = 100;
    sizedD.height = 100;
    sizedD.style.cssText = 'height:auto;aspect-ratio:1';
    document.body.appendChild(sizedD);

    const sizedReport12 = document.createElement('img');
    sizedReport12.src = 'data:image/png;report12';
    sizedReport12.loading = 'lazy';
    sizedReport12.width = 4000;
    sizedReport12.height = 1367;
    sizedReport12.style.cssText = 'width:100%;height:auto';
    document.body.appendChild(sizedReport12);

    // Force layout of sized images before appending the unsized image.
    document.body.offsetTop;

    // Truly unsized image that SHOULD trigger LazyLoadImageIssue
    const img = document.createElement('img');
    img.src = 'data:image/png,';
    img.loading = 'lazy';
    document.body.appendChild(img);
  `);
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})
