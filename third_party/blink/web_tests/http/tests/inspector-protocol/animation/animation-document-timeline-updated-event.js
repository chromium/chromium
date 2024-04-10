(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style type='text/css'>
    #node.anim {
        animation: anim 1s ease-in-out;
    }

    @keyframes anim {
        from {
            width: 100px;
        }
        to {
            width: 200px;
        }
    }
    </style>
    <div id='node' style='background-color: red; width: 100px'></div>
  `, 'Tests animationUpdated event for time based animations');

  dp.Animation.enable();
  session.evaluate('node.classList.add("anim")');
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  const animationUpdatedEvent = dp.Animation.onceAnimationUpdated();
  session.evaluate(`
    node.style.animationDuration = '3s';
  `);
  await animationUpdatedEvent;
  testRunner.log('Animation updated');
  testRunner.completeTest();
})
