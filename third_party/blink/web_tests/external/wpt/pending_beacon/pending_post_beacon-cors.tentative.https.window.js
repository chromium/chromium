// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

const {HTTPS_ORIGIN, HTTPS_NOTSAMESITE_ORIGIN} = get_host_info();
const SMALL_SIZE = 500;

for (const dataType in BeaconDataType) {
  postBeaconSendDataTest(
      dataType, generatePayload(SMALL_SIZE),
      `PendingPostBeacon[${dataType}]: same-origin`,
      {urlOptions: {host: HTTPS_ORIGIN, expectOrigin: HTTPS_ORIGIN}});

  postBeaconSendDataTest(
      dataType, generatePayload(SMALL_SIZE),
      `PendingPostBeacon[${dataType}]: cross-origin`, {
        urlOptions: {
          host: HTTPS_NOTSAMESITE_ORIGIN,
          expectOrigin: HTTPS_ORIGIN,
        }
      });
}

// TODO(crbug.com/1293679): Support preflight request for non CORS-safelisted
// Content-Type.
// TODO(crbug.com/1293679): Test that the browser rejects handling responses
// without appropriate CORS headers. You can test this with redirects.
// TODO(crbug.com/1293679): Test that the browser doesn't attach cookies to
// cross-origin requests.
