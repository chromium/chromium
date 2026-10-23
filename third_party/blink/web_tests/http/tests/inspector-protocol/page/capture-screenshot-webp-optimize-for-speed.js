// https://issues.chromium.org/issues/575911992
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        div { width: 400px; height: 300px; background: repeating-linear-gradient(45deg, #f00, #00f 10px, #0f0 20px); }
      </style>
      <div></div>
      <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>
    `,
      `Tests that Page.captureScreenshot optimizeForSpeed affects webp encoding`);

  const capture = async (quality, optimizeForSpeed) =>
      (await dp.Page.captureScreenshot(
           {format: 'webp', quality, optimizeForSpeed}))
          .result.data;

  const losslessSlow = await capture(100, false);
  const losslessFast = await capture(100, true);
  testRunner.log(
      losslessSlow.length < losslessFast.length ?
          'PASSED: slowly encoded lossless webp is smaller than quickly encoded' :
          `FAILED: ${losslessSlow.length} >= ${losslessFast.length}`);

  const lossySlow = await capture(80, false);
  const lossyFast = await capture(80, true);
  testRunner.log(
      lossySlow.length < lossyFast.length ?
          'PASSED: slowly encoded lossy webp is smaller than quickly encoded' :
          `FAILED: ${lossySlow.length} >= ${lossyFast.length}`);
  testRunner.completeTest();
})
