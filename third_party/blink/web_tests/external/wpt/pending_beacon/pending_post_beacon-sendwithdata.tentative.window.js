// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

// Test empty data.
for (const dataType in BeaconDataType) {
  postBeaconSendDataTest(
      dataType, '', `Sent empty ${dataType}, and server got no data.`, {
        expectNoData: true,
      });
}

// Test small payload.
for (const [dataType, skipCharset] of Object.entries(
         BeaconDataTypeToSkipCharset)) {
  postBeaconSendDataTest(
      dataType, generateSequentialData(0, 1024, skipCharset),
      'Encoded and sent in POST request.');
}

// TODO(crbug.com/1293679): Test large payload.
