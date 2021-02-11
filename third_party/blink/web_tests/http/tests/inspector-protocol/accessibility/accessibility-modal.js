(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <dialog data-dump id='modal'>
      <div data-dump role='button' aria-modal='true'>
      </div>
      <div data-dump role='dialog' aria-modal='false'>
      </div>
      <div data-dump role='dialog' aria-modal='true'>
      </div>
      <div data-dump role='alertdialog' aria-modal='true'>
      </div>
      <dialog data-dump>Closed Dialog</dialog>
      <dialog data-dump open>Open Dialog</dialog>
    </dialog>
  `, 'Tests accessibility values in modal dialog.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  await session.evaluate('document.getElementById("modal").showModal();');
  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
