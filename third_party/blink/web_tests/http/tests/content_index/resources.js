// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const swUrl = 'resources/sw.js';
const scope = 'resources/';

async function expectTypeErrorWithMessage(promise, message) {
  try {
    await promise;
    assert_unreached('Promise should have rejected');
  } catch (e) {
    assert_equals(e.name, 'TypeError');
    if (message) {
      assert_equals(e.message, message, 'Unexpected Error Message:');
    }
  }
}

function createDescription({id = 'id', title = 'title', description = 'description',
                            category = 'homepage', iconUrl = '/resources/square.png',
                            launchUrl = scope, includeIcons = true}) {
  return {id, title, description, category, icons: includeIcons ? [{src: iconUrl}] : [], launchUrl};
}

// Creates a Promise test for |func| given the |description|. The |func| will be
// executed with the `index` object of an activated Service Worker Registration.
function contentIndexTest(func, description) {
  promise_test(async t => {
    const registration = await service_worker_unregister_and_register(t, swUrl, scope);
    await wait_for_state(t, registration.installing, 'activated');
    return func(t, registration.index);
  }, description);
}

async function waitForMessageFromServiceWorker() {
  return await new Promise(resolve => {
    const listener = event => {
      navigator.serviceWorker.removeEventListener('message', listener);
      resolve(event.data);
    };

    navigator.serviceWorker.addEventListener('message', listener);
  });
}
