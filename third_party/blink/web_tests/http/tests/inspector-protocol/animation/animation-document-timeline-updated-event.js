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

  async function testAnimationUpdated(script, message) {
    const animationUpdatedEvent = dp.Animation.onceAnimationUpdated();
    session.evaluate(script);
    await animationUpdatedEvent;
    testRunner.log(message);
  }

  dp.Animation.enable();
  session.evaluate('node.classList.add("anim")');
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');

  await testAnimationUpdated(`node.style.animationDuration = '3s'`, 'Animation updated for duration change');
  await testAnimationUpdated(`node.style.animationIterationCount = 2`, 'Animation updated for iteration count change');
  await testAnimationUpdated(`node.style.animationTimingFunction = 'linear'`, 'Animation updated for keyframe easing change');
  await testAnimationUpdated(`
    const style = document.createElement('style');
    style.type = 'text/css';
    style.innerHTML = \`
      @keyframes anim {
        20% {
          width: 100px;
        }

        to {
          width: 200px;
        }
      }
    \`;
    document.querySelector('head').appendChild(style);
  `, 'Animation updated for keyframe offset change');
  testRunner.completeTest();
})
