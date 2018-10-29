<?php
// Generate token with the command:
// generate_token.py http://127.0.0.1:8000 BackgroundFetch --expire-timestamp=2000000000
header('Origin-Trial: AtDl/AukAuUX0Sw7KRz+mrV2vpSYrfDyVS4vdO3I1clqoNgKGqCX5Np5KIhlC6oQl8XcULXJz5bc9Y4CcYj9xA4AAABXeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQmFja2dyb3VuZEZldGNoIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9');
header('Content-Type: application/javascript');
?>
importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_properties(this, {
    'ServiceWorkerRegistration': ['backgroundFetch'],
    'BackgroundFetchFetch': ['request'],
    'BackgroundFetchManager': ['fetch', 'get', 'getIds'],
    'BackgroundFetchEvent': ['registration'],
    'BackgroundFetchUpdateUIEvent': ['updateUI'],
    'BackgroundFetchRecord': ['request', 'responseReady'],
    'BackgroundFetchRegistration': ['id', 'uploadTotal', 'uploaded',
                                    'downloadTotal', 'downloaded', 'result',
                                    'failureReason', 'recordsAvailable',
                                    'onprogress', 'abort', 'match',
                                    'matchAll'],
  });
});

test(t => {
  assert_true('onbackgroundfetchsuccess' in self, 'onbackgroundfetchsuccess property exists on global');
  assert_true('onbackgroundfetchfail' in self, 'onbackgroundfetchfail property exists on global');
  assert_true('onbackgroundfetchabort' in self, 'onbackgroundfetchabort property exists on global');
  assert_true('onbackgroundfetchclick' in self, 'onbackgroundfetchclick property exists on global');

}, 'Background Fetch API entry points in Origin-Trial enabled serviceworker.');

done();
