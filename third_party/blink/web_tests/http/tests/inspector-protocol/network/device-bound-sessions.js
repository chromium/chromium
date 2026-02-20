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
        dp.Network.enableDeviceBoundSessions({enable: true});
        const sessionsAddedEvent =
            await dp.Network.onceDeviceBoundSessionsAdded();
        testRunner.log('Received deviceBoundSessionsAdded event.');
        if (sessionsAddedEvent.params.sessions &&
            sessionsAddedEvent.params.sessions.length === 0) {
          testRunner.log('No sessions when initialized.');
        } else {
          testRunner.log(
              'ERROR: Event did not contain the expected session sessions.');
        }
      }

      const sessionId = 'dbsc-session-id';
      const cookieName = 'dbsc-cookie';
      testRunner.log('\n--- Creation event ---');
      {
        page.navigate(
            `https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/initiate-dbsc.php?session_id=${
                sessionId}&cookie_name=${cookieName}&domain_prefix=dbsc`);
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
            `fetch("/inspector-protocol/network/resources/dbsc/challenge.php?session_id=${
                sessionId}")`);
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

      await dp.Network.enableDeviceBoundSessions({enable: false});
    },

    async function testFailedRequestEvents() {
      dp.Network.enableDeviceBoundSessions({enable: true});
      await dp.Network.onceDeviceBoundSessionsAdded();

      const sessionId = 'dbsc-session-id-fail';
      const cookieName = 'dbsc-cookie-fail';

      testRunner.log('\n--- Creation failed request event ---');
      {
        const errorCode = 404;
        page.navigate(
            `https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/initiate-dbsc.php?session_id=${
                sessionId}&cookie_name=${
                cookieName}&domain_prefix=dbsc&error_code=${errorCode}`);
        const creationEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(
            creationEvent.params,
            'Creation failed request event: ', ['eventId']);
      }

      testRunner.log('\n--- Refresh failed request event ---');
      {
        page.navigate(`https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/initiate-dbsc.php?session_id=${
            sessionId}-refresh-fail&cookie_name=${
            cookieName}-refresh-fail&domain_prefix=dbsc&refresh_error_code=500`);
        await dp.Network.onceDeviceBoundSessionEventOccurred();
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/protected-resource.php")');
        const refreshEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(
            refreshEvent.params, 'Refresh failed request event: ', ['eventId']);
      }

      testRunner.log('\n--- Net error failed request event ---');
      {
        page.navigate(
            `https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/initiate-dbsc.php?session_id=${
                sessionId}-net-err&cookie_name=${
                cookieName}-net-err&domain_prefix=dbsc&reg_url=${
                encodeURIComponent('https://dbsc.test:1/registration.php')}`);
        const creationEvent =
            await dp.Network.onceDeviceBoundSessionEventOccurred();
        testRunner.log(
            creationEvent.params,
            'Failed request with net error event: ', ['eventId']);
      }

      // Clear sessions.
      session.evaluate(
          'fetch("/inspector-protocol/network/resources/dbsc/termination.php")');
      await dp.Network.onceDeviceBoundSessionEventOccurred();

      await dp.Network.enableDeviceBoundSessions({enable: false});
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
    },

    async function testRequestWillBeSentExtraInfo() {
      testRunner.log('\n--- Make sure initial set of sessions is empty ---');
      {
        dp.Network.enableDeviceBoundSessions({enable: true});
        const sessionsAddedEvent =
            await dp.Network.onceDeviceBoundSessionsAdded();
        testRunner.log(`Num sessions received: ${
            sessionsAddedEvent.params.sessions.length}`);
      }

      const sessionId1 = 'dbsc-session-id1';
      const cookieName1 = 'dbsc-cookie1';
      const sessionId2 = 'dbsc-session-id2';
      const cookieName2 = 'dbsc-cookie2';
      const sessionId3 = 'dbsc-session-id3';
      const cookieName3 = 'dbsc-cookie3';
      testRunner.log(
          '\n--- Set up four sessions (one for a different site) ---');
      {
        let numSuccessfulCreationEvents = 0;
        async function createSession(sessionId, cookieName, domainPrefix) {
          page.navigate(`https://${
              domainPrefix}.test:8443/inspector-protocol/network/resources/dbsc/initiate-dbsc.php?session_id=${
              sessionId}&cookie_name=${cookieName}&domain_prefix=${
              domainPrefix}`);
          const event = await dp.Network.onceDeviceBoundSessionEventOccurred();
          if (event.params.succeeded && event.params.creationEventDetails) {
            numSuccessfulCreationEvents++;
          }
        }
        await createSession(sessionId1, cookieName1, 'dbsc');
        await createSession(sessionId2, cookieName2, 'dbsc');
        await createSession(sessionId3, cookieName3, 'dbsc');
        await createSession(sessionId1, cookieName1, 'dbsc-alternate');
        testRunner.log(
            `Sessions added successfully: ${numSuccessfulCreationEvents}`);
      }

      testRunner.log(
          '\n--- Trigger deferred refresh for session 1, proactive refresh for session 2, nothing for session 3 ---');
      {
        await page.navigate(
            `https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/set-cookie.php?cookie_name=${
                cookieName2}&max_age=60&domain_prefix=dbsc`);
        await page.navigate(
            `https://dbsc.test:8443/inspector-protocol/network/resources/dbsc/set-cookie.php?cookie_name=${
                cookieName3}&max_age=600&domain_prefix=dbsc`);
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/protected-resource.php")');
        const requestExtraInfo =
            await dp.Network.onceRequestWillBeSentExtraInfo();
        testRunner.log(
            requestExtraInfo.params.deviceBoundSessionUsages,
            'Usages (only same-site included): ', []);
      }

      testRunner.log(
          '\n--- Trigger deferred refresh for session 1 for alternate site ---');
      {
        await page.navigate(
            `https://dbsc-alternate.test:8443/inspector-protocol/network/resources/dbsc/set-cookie.php?cookie_name=${
                cookieName1}&max_age=0&domain_prefix=dbsc`);
        session.evaluate(
            'fetch("/inspector-protocol/network/resources/dbsc/protected-resource.php")');
        const requestExtraInfo =
            await dp.Network.onceRequestWillBeSentExtraInfo();
        testRunner.log(
            requestExtraInfo.params.deviceBoundSessionUsages,
            'Usages (only same-site for alternate site included): ', []);
      }
    }
  ]);
})
