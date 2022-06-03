(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <input type='text'></input>
  `, 'Tests that node without AXNode reports accessibility values.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('head', false, msg);
})
