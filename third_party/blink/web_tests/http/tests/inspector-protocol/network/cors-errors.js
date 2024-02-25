(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure CORS errors are correctly reported.`);

  // This url should be cross origin.
  const url =
      `https://127.0.0.1:8443/inspector-protocol/network/resources/cors-headers.php`;

  await dp.Network.enable();

  const failures = [];
  let checkComplete;
  let completion = new Promise(r => {
    checkComplete = () => failures.length == 3 && r();
  });

  dp.Network.onLoadingFailed(event => {
    failures.push(event.params);
    checkComplete();
  });

  session.evaluate(`
    fetch('${url}');
  `);
  session.evaluate(`
    fetch('${url}?origin=${encodeURIComponent('http://127.0.0.1')}');
  `);
  session.evaluate(`
    fetch("${
      url}?methods=GET&origin=1", {method: 'POST', mode: 'cors', body: 'FOO', cache: 'no-cache',  headers: { 'Content-Type': 'application/json'} });
  `);

  await completion;
  failures.sort(
      (a, b) => a.corsErrorStatus.corsError.localeCompare(
          b.corsErrorStatus.corsError));
  for (const failure of failures) {
    testRunner.log(failure);
  }
  testRunner.completeTest();
})
