
import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
(async function() {
  TestRunner.addResult(
      `Tests that User-Agent override works for requests from Service Workers.\n`);
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('resources');

  // Suppress racy error message: "error: Connection is closed, can't dispatch pending call"
  // TODO(jarhar): Fix the root cause of this error message here and in workers-on-navigation.js
  console.error = () => {};

  const testPage =
      'http://localhost:8000/devtools/service-workers/resources/sw-return-useragent.php';
  SDK.NetworkManager.MultitargetNetworkManager.instance().setUserAgentOverride(
      'Mozilla/5.0 (Overridden User Agent)');

  const targetAdded = TestRunner.waitForTarget(
      target => target.type() === SDK.Target.Type.ServiceWorker);

  await TestRunner.navigatePromise(testPage);
  TestRunner.addResult('navigated to ' + testPage);
  TestRunner.addResult(
      'user-agent: ' +
      await TestRunner.evaluateInPagePromise('document.body.innerText'));
  const target = await targetAdded;
  const targetRemoved = TestRunner.waitForTargetRemoved(target);
  TestRunner.addResult('awaited service worker target created');

  const navigateAwayPage = 'http://127.0.0.1:8000';
  await TestRunner.navigatePromise(navigateAwayPage);
  TestRunner.addResult('navigated to ' + navigateAwayPage);
  TestRunner.addResult('');

  const registrations = TestRunner.serviceWorkerManager.registrations();
  for (const registrationId of registrations.keys()) {
    const registration = registrations.get(registrationId);
    for (const serviceWorkerVersion of registration.versions.values()) {
      const versionId = serviceWorkerVersion.id;
      TestRunner.serviceWorkerManager.stopWorker(versionId);
    }
  }
  await targetRemoved;
  TestRunner.addResult('Stopped worker and awaited target removal');

  await TestRunner.navigatePromise(testPage);
  TestRunner.addResult('navigated to ' + testPage);
  TestRunner.addResult(
      'user-agent: ' +
      await TestRunner.evaluateInPagePromise('document.body.innerText'));

  TestRunner.completeTest();
})();
