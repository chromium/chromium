(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <input data-dump aria-errormessage='err'>
    <h3 id='err'>This text field has an error!</h3>

    <img data-dump aria-details='d' aria-label='Label'>
    <div id='d'>Details</div>

    <button data-dump aria-keyshortcuts='Ctrl+A'>Select All</button>

    <input data-dump type='checkbox' aria-roledescription='Lightswitch' checked>
  `, 'Tests ARIA 1.1 accessibility markup.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
