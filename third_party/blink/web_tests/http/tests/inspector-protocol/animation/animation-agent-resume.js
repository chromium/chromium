(async function(testRunner) {
  // This test provides coverage for http://crbug.com/999066 -
  // "Animations are not being captured when DevTools are already open".
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that animation agent resumes post navigation.`);

  // Starts the InspectorAnimationAgent, *before* navigating.
  await dp.Animation.enable();

  // This navigation starts a new renderer, so the animation agent must resume.
  // If it doesn't, the test below will time out because we'll never receive
  // the animation started event.
  await session.navigate('./resources/simple.html');

  // Trigger an animation and observe it. We will only be able to
  // observe if the animation agent was restarted after the navigation.
  dp.Animation.onAnimationStarted((event) => {
    testRunner.log('Animation started: ' + event.params.animation.name);
    testRunner.completeTest();
  });
  session.evaluate(`
      const div = document.createElement('div');
      div.setAttribute('style', 'background-color: red; height: 100px');
      document.body.appendChild(div);
      div.animate([{ width: "100px" },{ width: "200px" }],
                   { duration: 200, delay: 100, id: "yay!" });`);
})
