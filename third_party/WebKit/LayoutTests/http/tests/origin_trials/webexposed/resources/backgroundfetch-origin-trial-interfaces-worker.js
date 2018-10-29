importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {

  OriginTrialsHelper.check_properties(this, {
       'ServiceWorkerRegistration': ['backgroundFetch'],
       'BackgroundFetchFetch': ['request'],
       'BackgroundFetchManager': ['fetch', 'get', 'getIds'],
       'BackgroundFetchRegistration': ['id', 'uploadTotal', 'uploaded',
                                       'downloadTotal', 'downloaded', 'result',
                                       'failureReason', 'recordsAvailable',
                                       'onprogress'],
       });
}, 'Background Fetch API interfaces in an Origin-Trial enabled worker.');

done();
