// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The "onsync" event currently understands commands (passed as
// registration tags) coming from the test. Any other tag is
// passed through to the document unchanged.
//
// "delay" - Delays finishing the sync event with event.waitUntil.
//           Send a postMessage of "completeDelayedRegistration" to finish the
//           event.

'use strict';

let resolveCallback = null;
let rejectCallback = null;
let periodicSyncEventCount = 0;

this.onmessage = (event) => {
  switch(event.data.action) {
    case 'completeDelayedSyncEvent': {
      if (resolveCallback === null) {
        sendMessageToClients('sync', 'error - resolveCallback is null');
        return;
      }

      periodicSyncEventCount++;
      resolveCallback();
      sendMessageToClients('sync', 'ok - delay completed');
      return;
    }
    case 'rejectDelayedSyncEvent': {
      if (rejectCallback === null) {
        sendMessageToClients('sync', 'error - rejectCallback is null');
        return;
      }

      rejectCallback();
      sendMessageToClients('sync', 'ok - delay rejected');
    }
    case 'registerOneShotSync': {
      const tag = event.data.tag;
      registration.sync.register(tag)
        .then(() => {
          sendMessageToClients('register', 'ok - ' + tag + ' registered in SW');
        })
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'registerPeriodicSync': {
      const tag = event.data.tag;
      const minInterval = event.data.minInterval;
      registration.periodicSync.register(tag, { minInterval: minInterval })
        .then(() =>
          sendMessageToClients('register', 'ok - ' + tag + ' registered in SW'))
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'unregister': {
      const tag = event.data.tag;
      registration.periodicSync.unregister(tag)
        .then(() =>
          sendMessageToClients(
            'unregister', 'ok - ' + tag + ' unregistered in SW'))
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'hasOneShotSyncTag': {
      const tag = event.data.tag;
      registration.sync.getTags()
        .then((tags) => {
          if (tags.indexOf(tag) >= 0) {
            sendMessageToClients('register', 'ok - ' + tag + ' found');
          } else {
            sendMessageToClients('register', 'error - ' + tag + ' not found');
            return;
          }
        })
        .catch(sendSyncErrorToClients);
        return;
      }
    case 'hasPeriodicSyncTag': {
      const tag = event.data.tag;
      registration.periodicSync.getTags()
        .then((tags) => {
          if (tags.indexOf(tag) >= 0) {
            sendMessageToClients('register', 'ok - ' + tag + ' found in SW');
          } else {
            sendMessageToClients('register', 'error - ' + tag + ' not found');
            return;
          }
        })
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'getOneShotSyncTags': {
      registration.sync.getTags()
        .then((tags) => {
          sendMessageToClients('register', 'ok - ' + tags.toString());
        })
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'getPeriodicSyncTags': {
      registration.periodicSync.getTags()
        .then((tags) => {
          sendMessageToClients('register', 'ok - ' + tags.toString());
        })
        .catch(sendSyncErrorToClients);
      return;
    }
    case 'getPeriodicSyncEventCount':
      sendMessageToClients('gotEventCount', periodicSyncEventCount.toString());
      return;
  }
}

function handleSync(event, isPeriodic) {
  const expectedEventType = isPeriodic ? 'PeriodicSyncEvent' : 'SyncEvent';

  const eventProperties = [
    // Extract name from toString result: "[object <Class>]"
    Object.prototype.toString.call(event).match(/\s([a-zA-Z]+)/)[1],
    (typeof event.waitUntil)
  ];

  if (eventProperties[0] !== expectedEventType) {
    return sendMessageToClients('sync', 'error - wrong event type');
  }

  if (eventProperties[1] != 'function') {
    return sendMessageToClients('sync', 'error - wrong wait until type');
  }

  if (event.tag === undefined) {
    return sendMessageToClients('sync', 'error - registration missing tag');
  }

  const tag = event.tag;

  if (tag === 'delay') {
    const syncPromise = new Promise((resolve, reject) => {
      resolveCallback = resolve;
      rejectCallback = reject;
    });
    return syncPromise;
  }

  if (isPeriodic)
    periodicSyncEventCount++;
  return sendMessageToClients('sync', tag + ' fired');
}

self.addEventListener('sync', event => {
  event.waitUntil(handleSync(event, /* isPeriodic= */ false));
});

self.addEventListener('periodicsync', event => {
  event.waitUntil(handleSync(event, /* isPeriodic= */ true));
});

function sendMessageToClients(type, data) {
  return clients.matchAll({ includeUncontrolled: true }).then((clients) => {
    clients.forEach((client) => {
      client.postMessage({type, data});
    });
  }, (error) => {
    console.log(error);
  });
}

function sendSyncErrorToClients(error) {
  sendMessageToClients('sync', error.name + ' - ' + error.message);
}
