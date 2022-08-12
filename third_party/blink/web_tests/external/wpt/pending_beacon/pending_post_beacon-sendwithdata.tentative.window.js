// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

// Test empty data.
postBeaconSendDataTest(
    BeaconDataType.String, '',
    /*expectNoData=*/ true, 'Sent empty String, and server got no data.');
postBeaconSendDataTest(
    BeaconDataType.ArrayBuffer, '',
    /*expectNoData=*/ true, 'Sent empty ArrayBuffer, and server got no data.');
postBeaconSendDataTest(
    BeaconDataType.FormData, '',
    /*expectNoData=*/ false, 'Sent empty form payload, and server got "".');
postBeaconSendDataTest(
    BeaconDataType.URLSearchParams, 'testkey=',
    /*expectNoData=*/ false, 'Sent empty URLparams, and server got "".');

// Test small payload.
postBeaconSendDataTest(
    BeaconDataType.String, generateSequentialData(0, 1024),
    /*expectNoData=*/ false, 'Encoded and sent in POST request.');
postBeaconSendDataTest(
    BeaconDataType.ArrayBuffer, generateSequentialData(0, 1024),
    /*expectNoData=*/ false, 'Encoded and sent in POST request.');
// Skip CRLF characters which will be normalized by FormData.
postBeaconSendDataTest(
    BeaconDataType.FormData, generateSequentialData(0, 1024, '\n\r'),
    /*expectNoData=*/ false, 'Encoded and sent in POST request.');
// Skip reserved URI characters.
postBeaconSendDataTest(
    BeaconDataType.URLSearchParams,
    'testkey=' + generateSequentialData(0, 1024, ';,/?:@&=+$'),
    /*expectNoData=*/ false, 'Encoded and sent in POST request.');

// TODO(crbug.com/1293679): Test large payload.
