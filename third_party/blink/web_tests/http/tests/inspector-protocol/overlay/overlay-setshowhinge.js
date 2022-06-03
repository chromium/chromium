(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests the new Overlay.setShowHinge method.');

  let pingResponse = await dp.Overlay.setShowHinge({
    hingeConfig: {
      rect: {x: 10, y: 0, width: 100, height: 300},
      contentColor: {r: 38, g: 38, b: 38, a: 1}
    }
  });
  testRunner.log(pingResponse);

  pingResponse = await dp.Overlay.setShowHinge({
    hingeConfig: {
      rect: {x: -10, y: 0, width: 100, height: 300},
      contentColor: {r: 38, g: 38, b: 38, a: 1}
    }
  });
  testRunner.log(pingResponse);

  pingResponse = await dp.Overlay.setShowHinge({});
  testRunner.log(pingResponse);

  testRunner.completeTest();
})