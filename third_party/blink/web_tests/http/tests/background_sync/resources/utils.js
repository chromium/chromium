'use strict';

// Depends on /shared_utils.js
function periodicSyncTest(func, description) {
  promise_test(async test => {
    await PermissionsHelper.setPermission(
      'periodic-background-sync', 'granted');
    const serviceWorkerRegistration =
      await registerAndActivateServiceWorker(test);
    return func(test, serviceWorkerRegistration.periodicSync);
  }, description);
}
