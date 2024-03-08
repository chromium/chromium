//META: script=/resources/testharness.js
//META: script=/resources/testharnessreport.js
//META: script=../resources/feature-policy-permissions-test.js

import {GeolocationMock} from '/resources/geolocation-mock.js';

const mockLatitude = 51.478;
const mockLongitude = -0.166;
const mockAccuracy = 100.0;

const mock = new GeolocationMock();
mock.setGeolocationPermission(true);
mock.setGeolocationPosition(mockLatitude, mockLongitude, mockAccuracy);

run_permission_default_header_policy_tests(
  location.protocol + '//localhost:' + location.port,
  'geolocation',
  'geolocation',
  'GeolocationPositionError',
  function() { return new Promise((resolve, reject) => {
    navigator.geolocation.getCurrentPosition(resolve, reject); }); });
