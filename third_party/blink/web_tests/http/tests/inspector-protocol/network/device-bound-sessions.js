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

      // TODO(crbug.com/471017387): Once event listeners exist, check populated
      // set of sessions as well.
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
