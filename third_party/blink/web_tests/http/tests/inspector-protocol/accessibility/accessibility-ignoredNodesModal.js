(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
    body.done * {
      display: none;
    }
    </style>
    <div data-dump>Hidden by modal dialog</div>
    <dialog data-dump>
        (Dialog itself is not hidden)
        <div data-dump>Non-hidden element inside dialog</div>
    </dialog>
  `, 'Tests accessibility values of ignored nodes in model dialog.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  await session.evaluate('document.querySelector("dialog").showModal();');
  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
