// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/common/utils.js
// META: script=./resources/pending_beacon-helper.js

'use strict';

promise_test(async t => {
  const uuid = token();

  // Create an iframe that contains a document that creates two beacons.
  const iframe = document.createElement('iframe');
  iframe.src =
      `/pending_beacon/resources/iframe_create_beacon_no_send.html?uuid=${
          uuid}`;

  const iframe_load_promise =
      new Promise(resolve => iframe.onload = () => resolve());
  document.body.appendChild(iframe);
  await iframe_load_promise;

  // Delete the iframe to trigger beacon sending.
  document.body.removeChild(iframe);

  // The iframe should have sent two beacons.
  await expectBeacon(uuid, {count: 2});
}, 'Verify that a discarded document sends its beacons.');

promise_test(async t => {
  const uuid = token();

  // Create an iframe that contains a document that creates a beacon,
  // then sends it with `sendNow()`
  const iframe = document.createElement('iframe');
  iframe.src =
      `/pending_beacon/resources/iframe_create_beacon_then_send.html?uuid=${
          uuid}`;

  const iframe_load_promise =
      new Promise(resolve => iframe.onload = () => resolve());
  document.body.appendChild(iframe);
  await iframe_load_promise;

  // The iframe should have sent one beacon.
  await expectBeacon(uuid, {count: 1});

  // Delete the document and verify no more beacons are sent.
  document.body.removeChild(iframe);
  await expectBeacon(uuid, {count: 1});
}, 'Verify that a discarded document does not send an already sent beacon.');
