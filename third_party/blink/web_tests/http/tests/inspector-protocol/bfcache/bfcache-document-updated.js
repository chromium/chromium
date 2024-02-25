(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
    'http://localhost:8000/inspector-protocol/resources/test-page.html',
    'Tests that document is updated after a BFCache navigation');
  await dp.DOM.enable();

  // Regular navigation.
  session.navigate('https://devtools.oopif.test:8443/inspector-protocol/resources/iframe.html');
  await dp.DOM.onceDocumentUpdated();
  testRunner.log('documentUpdated received after regular navigation');

  // Navigate back - should use back-forward cache.
  session.evaluate('window.history.back()');
  await dp.DOM.onceDocumentUpdated();
  testRunner.log('documentUpdated received after bfcache navigation');

  testRunner.completeTest();
});
