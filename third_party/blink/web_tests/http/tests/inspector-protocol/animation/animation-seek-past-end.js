(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <script src='../../resources/run-after-layout-and-paint.js'></script>
    <div id='node' style='background-color: red; height: 100px; width: 100px'></div>
  `, 'Tests seeking animation past end time.');

  dp.Animation.enable();
  session.evaluate(`
    node.animate([{ width: '1000px' }, { width: '2000px' }], 1000);
  `);

  var response = await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  await dp.Animation.seekAnimations({ animations: [ response.params.animation.id ], currentTime: 2000 });

  var rafWidth = await session.evaluateAsync(`
    (function rafWidth() {
        var callback;
        var promise = new Promise((fulfill) => callback = fulfill);
        runAfterLayoutAndPaint(() => callback(node.offsetWidth));
        return promise;
    })()
  `);
  testRunner.log(rafWidth);
  testRunner.completeTest();
})
