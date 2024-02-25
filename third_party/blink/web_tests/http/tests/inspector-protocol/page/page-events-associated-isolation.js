(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <a href="">
      <div style="width:1000px; height: 1000px" href="http://127.0.0.1:8000/inspector-protocol/resources/blank.html">LINK</div>
    </a>
  `, 'Tests that page events are associated with the input');

  await dp.Page.enable();
  dp.Page.onFrameRequestedNavigation(event => testRunner.log(event));

  // Click.
  await dp.Input.dispatchMouseEvent({type: 'mouseMoved', button: 'left', buttons: 0, clickCount: 1, x: 150, y: 150 });
  await dp.Input.dispatchMouseEvent({type: 'mousePressed', button: 'left', buttons: 0, clickCount: 1, x: 150, y: 150 });
  testRunner.log('Before release');
  await dp.Input.dispatchMouseEvent({type: 'mouseReleased', button: 'left', buttons: 1, clickCount: 1, x: 150, y: 150 });
  testRunner.log('After release');

  testRunner.completeTest();
})
