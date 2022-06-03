'use strict';

function loadScript(path) {
  let script = document.createElement('script');
  let promise = new Promise(resolve => script.onload = resolve);
  script.src = path;
  script.async = false;
  document.head.appendChild(script);
  return promise;
}

function backgroundFetchTest(func, description) {
  promise_test(async t => {
    if (typeof PermissionsHelper === 'undefined') {
      await loadScript('/resources/permissions-helper.js');
    }
    await PermissionsHelper.setPermission('background-fetch', 'granted');

    if (typeof registerAndActivateServiceWorker === 'undefined') {
      await loadScript('../serviceworker/resources/shared-utils.js');
    }
    const serviceWorkerRegistration = await registerAndActivateServiceWorker(t);
    return func(t, serviceWorkerRegistration.backgroundFetch);
  }, description);
}

let _nextBackgroundFetchTag = 0;
function uniqueTag() {
  return 'tag' + _nextBackgroundFetchTag++;
}
