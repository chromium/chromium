// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline events for module compile & evaluate.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      async function performActions() {
        const key = await window.crypto.subtle.generateKey({name: "AES-CBC", length: 256}, false, ["encrypt", "decrypt"]);
        const data = new Uint8Array(512);
        const iv = window.crypto.getRandomValues(new Uint8Array(16));
        const crypted = await window.crypto.subtle.encrypt({name: "AES-CBC", iv}, key, data);
        return window.crypto.subtle.decrypt({name: "AES-CBC", iv}, key, crypted);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  const events = new Set([
    TimelineModel.TimelineModel.RecordType.CryptoDoEncrypt,
    TimelineModel.TimelineModel.RecordType.CryptoDoEncryptReply,
    TimelineModel.TimelineModel.RecordType.CryptoDoDecrypt,
    TimelineModel.TimelineModel.RecordType.CryptoDoDecryptReply
  ]);
  const tracingModel = PerformanceTestRunner.tracingModel();
  const eventsToPrint = [];
  tracingModel.sortedProcesses().forEach(p => p.sortedThreads().forEach(t =>
      eventsToPrint.push(...t.events().filter(event => events.has(event.name)))));

  for (const event of eventsToPrint) {
    await PerformanceTestRunner.printTraceEventPropertiesWithDetails(event);
  }
  TestRunner.completeTest();
})();
