// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  'use strict';
  const {page, session, dp} = await testRunner.startBlank(
      'Tests device-bound-sessions protocol additions.');
  await dp.Network.enable();
  testRunner.runTestSuite([
    async function testSessionsAndEvents() {
      testRunner.log('\n--- Checking initial set of sessions is empty ---');
      {
        const sessionsAddedEventPromise =
            dp.Network.onceDeviceBoundSessionsAdded();
        dp.Network.enableDeviceBoundSessions({enable: true});
        const sessionsAddedEvent = await sessionsAddedEventPromise;
        testRunner.log('Received deviceBoundSessionsAdded event.');
        if (sessionsAddedEvent.params.sessions &&
            sessionsAddedEvent.params.sessions.length === 0) {
          testRunner.log('No sessions when initialized.');
        } else {
          testRunner.log(
              'ERROR: Event did not contain the expected session sessions.');
        }
      }

      testRunner.log('\n--- Creation event ---');
      {
        page.navigate(
            'http://localhost:8080/inspector-protocol/network/resources/dbsc/initiate-dbsc.php');
        const creationEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(
            creationEvent.params,
            'Creation event: ', ['expiryDate', 'eventId']);
      }

      testRunner.log('\n--- Reset tracking device bound session events ---');
      {
        await dp.Network.enableDeviceBoundSessions({enable: false});
        testRunner.log('Device Bound Sessions disabled in DevTools.');
      }

      testRunner.log('\n--- Checking new set of sessions has one session ---');
      {
        dp.Network.enableDeviceBoundSessions({enable: true});
        testRunner.log('Re-enabling Device Bound Sessions in DevTools.');

        const sessionAddedEvent =
            await dp.Network.onceDeviceBoundSessionsAdded();
        testRunner.log(
            sessionAddedEvent.params, 'New sessions: ', ['expiryDate']);
      }

      testRunner.log('\n--- Trigger refresh event ---');
      {
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/protected-resource.php")');
        const refreshEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(refreshEvent.params, 'Refresh event: ', ['eventId']);
      }

      testRunner.log('\n--- Trigger challenge event ---');
      {
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/challenge.php")');
        const challengeEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(challengeEvent.params, 'Challenge event: ', ['eventId']);
      }

      testRunner.log('\n--- Trigger termination event ---');
      {
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/termination.php")');
        const terminationEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(
            terminationEvent.params, 'Termination event: ', ['eventId']);
      }
    },

    async function testFetchSchemefulSite() {
      const testCases = [
        'https://www.google.com/path',
        'http://localhost:8080',
        'https://192.168.1.1/test',
        'file:///path/to/file.html',
      ];
      for (const origin of testCases) {
        const {result} = await dp.Network.fetchSchemefulSite({origin});
        testRunner.log(result);
      }
    }
  ]);
})
