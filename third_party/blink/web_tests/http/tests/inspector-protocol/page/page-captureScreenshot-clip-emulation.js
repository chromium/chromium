(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
    div.to-screenshot {
      border: 2px solid blue;
      background: green;
      width: 49px;
      height: 49px;
    }
    html, body {
      margin: 0;
      padding: 0;
    }
    </style>
    <div class="to-screenshot"></div>`, 'Tests that screenshot works with clip and emulation');

  testRunner.log(await dp.Emulation.setDeviceMetricsOverride({width: 200, height: 200, deviceScaleFactor: 0, mobile: false}));

  const json = await session.evaluate(`
    (() => {
      const div = document.querySelector('div.to-screenshot');
      const box = div.getBoundingClientRect();
      return JSON.stringify({x: box.left, y: box.top, width: box.width, height: box.height, dpr: window.devicePixelRatio});
    })()
  `);

  const parsed = JSON.parse(json);
  const dpr = parsed.dpr;
  const clip = { ...parsed, dpr: undefined };
  testRunner.log(clip);

  testRunner.log(await dp.Page.captureScreenshot({format: 'png', clip: {...clip, scale: 1 / dpr}}));
  testRunner.log(await dp.Page.captureScreenshot({format: 'png', clip: {...clip, scale: 3 / dpr}}));
  testRunner.log(await dp.Page.captureScreenshot({format: 'png', clip: {...clip, scale: 0.5 / dpr}}));

  testRunner.completeTest();
})
