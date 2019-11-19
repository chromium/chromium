(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that data: URLs are not intercepted.`);

  var FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  var helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [],
    body: btoa("overriden response body")
  });

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({enabled: true});
  await dp.Page.navigate({url: 'data:text/html,<html>hello</html>'});
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  let body = await session.evaluate(`document.body.textContent`);
  testRunner.log(`document.body: ${body}`);

  const url = 'data:text/plain,subresource content';
  let content = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
  testRunner.log(`Response after Fetch.fulfillRequest: ${content}`);

  testRunner.completeTest();
})
