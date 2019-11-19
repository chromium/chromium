'use strict';

// Depends on /shared_utils.js
function backgroundFetchTest(func, description) {
  promise_test(async t => {
    const serviceWorkerRegistration = await registerAndActivateServiceWorker(t);
    return func(t, serviceWorkerRegistration.backgroundFetch);
  }, description);
}

let _nextBackgroundFetchTag = 0;
function uniqueTag() {
  return 'tag' + _nextBackgroundFetchTag++;
}