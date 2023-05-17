// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`fromPrefetchCache flag must be set for prefetched resousces.\n`);

  await TestRunner.showPanel('network');
  const ret = await TestRunner.evaluateInPageAsync(`
    (function(){
      return new Promise(resolve => {
        const link = document.createElement('link');
        link.rel = 'prefetch';
        link.href = 'resources/network-prefetch-target.html';
        link.addEventListener('load', resolve);
        document.body.appendChild(link);
      });
    })();
  `);
  NetworkTestRunner.recordNetwork();
  await TestRunner.addIframe('resources/network-prefetch-target.html');
  var request1 = NetworkTestRunner.networkRequests().pop();
  TestRunner.addResult(request1.url());
  TestRunner.addResult('fromPrefetchCache: ' + request1.fromPrefetchCache());
  TestRunner.completeTest();
})();
