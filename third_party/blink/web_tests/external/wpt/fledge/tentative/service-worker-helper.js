"use strict;"

const privateRequests = ['resources/trusted-bidding-signals.py',
  'update-url.py',
  'wasm-helper.py',
  'bidding-logic.py',
  'decision-logic.py',
  'trusted-scoring-signals.py',
  'trusted-bidding-signals.py',
  'bidder_report',
  'seller_report'];
const publicRequests = ['direct-from-seller-signals.py'];

// The service worker intercepts fetch calls and uses
// pre-defined lists to categorize requests as "public"
// or "private". It broadcasts a success/failure status
// on the appropriate BroadcastChannel.
self.addEventListener('fetch', (event) => {
  const privateRequestChannel = new BroadcastChannel('private-requests-test');
  const publicRequestChannel = new BroadcastChannel('public-requests-test');

  privateRequests.forEach(privateRequest => {
    if (event.request.url.includes(privateRequest)) {
      privateRequestChannel.postMessage({
        result: 'failed',
        message: "service worker should not have seen: " +
          privateRequest
      });
    }
  });

  publicRequests.forEach(publicRequest => {
    if (event.request.url.includes(publicRequest)) {
      publicRequestChannel.postMessage({
        result: 'passed',
        message: "service worker properly saw: " +
          publicRequest
      });
    }
  });

  // This endpoint is specifically for the 'private-requests-test' to signal
  // completion and ensure proper BroadcastChannel response behavior.
  if (event.request.url.includes('COMPLETE-TEST')) {
    privateRequestChannel.postMessage({result:'completed'});
  }
});

