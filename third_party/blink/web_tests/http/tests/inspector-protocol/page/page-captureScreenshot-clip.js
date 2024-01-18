(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>div.above {
      border: 2px solid blue;
      background: red;
      height: 15000px;
    }
    div.to-screenshot {
      border: 2px solid blue;
      background: green;
      width: 50px;
      height: 50px;
    }
    body {
      margin: 0;
    }
    </style>
    <div style="height: 14px">oooo</div>
    <div class="above"></div>
    <div class="to-screenshot"></div>`, 'Tests that screenshot works with clip');

  const clip = await session.evaluate(`
    (() => {
      const div = document.querySelector('div.to-screenshot');
      const box = div.getBoundingClientRect();
      window.scrollTo(0, 15000);
      return {x: box.left, y: box.top, width: box.width, height: box.height};
    })()
  `);

  async function test(scale) {
    const params = {format: 'png', clip: {...clip, scale: scale}};
    testRunner.log(JSON.stringify(params));
    testRunner.log(await dp.Page.captureScreenshot(params));
  }

  await test(1);
  await test(0.5);
  await test(2);

  testRunner.completeTest();
})
