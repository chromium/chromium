// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const resultQueue = new ResultQueue();

// Sends data back to the test. This must be in response to an earlier
// request, but it's ok to respond asynchronously. The request blocks until
// the response is sent.
function sendResultToTest(result) {
  console.log('sendResultToTest: ' + result);
  if (window.domAutomationController) {
    domAutomationController.send('' + result);
  }
}

function sendErrorToTest(error) {
  sendResultToTest(error.name + ' - ' + error.message);
}

function registerServiceWorker() {
  navigator.serviceWorker.register('service_worker.js', {scope: './'})
    .then(() => {
      return navigator.serviceWorker.ready;
    })
    .then((swRegistration) => {
      sendResultToTest('ok - service worker registered');
    })
    .catch(sendErrorToTest);
}

function registerOneShotSync(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.register(tag);
    })
    .then(() => {
      sendResultToTest('ok - ' + tag + ' registered');
    })
    .catch(sendErrorToTest);
}

function registerOneShotSyncFromLocalFrame(frame_url) {
  let frameWindow;
  return createFrame(frame_url)
    .then((frame) => {
      frameWindow = frame.contentWindow;
      return frameWindow.navigator.serviceWorker.register('service_worker.js');
    })
    .then(() => {
      return frameWindow.navigator.serviceWorker.ready;
    })
    .then((frame_registration) => {
      return frame_registration.sync.register('foo');
    })
    .then(() => {
      sendResultToTest('ok - iframe registered sync');
    })
    .catch(sendErrorToTest);
}

function registerOneShotSyncFromCrossOriginFrame(cross_frame_url) {
  return createFrame(cross_frame_url)
    .then((frame) => {
      return receiveMessage();
    })
    .then((message) => {
      console.log(message);
      if (message !== 'registration failed') {
        sendResultToTest('failed - ' + message);
        return;
      }
      sendResultToTest('ok - frame failed to register sync');
    });
}

function registerOneShotSyncFromServiceWorker(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'registerOneShotSync', tag: tag});
      sendResultToTest('ok - ' + tag + ' register sent to SW');
    })
    .catch(sendErrorToTest);
}

function registerPeriodicSync(tag, minInterval) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      if (minInterval !== undefined) {
        return swRegistration.periodicSync.register(
            tag, {minInterval: minInterval});
      } else {
        return swRegistration.periodicSync.register(tag);
      }
    })
    .then(() => {
      sendResultToTest('ok - ' + tag + ' registered');
    })
    .catch(sendErrorToTest);
}

function registerPeriodicSyncFromLocalFrame(frame_url) {
  let frameWindow;
  return createFrame(frame_url)
    .then((frame) => {
      frameWindow = frame.contentWindow;
      return frameWindow.navigator.serviceWorker.register('service_worker.js');
    })
    .then(() => {
      return frameWindow.navigator.serviceWorker.ready;
    })
    .then((frame_registration) => {
      return frame_registration.periodicSync.register(
          'foo', {});
    })
    .then(() => {
      sendResultToTest('ok - iframe registered periodicSync');
    })
    .catch(sendErrorToTest);
}

function registerPeriodicSyncFromCrossOriginFrame(cross_frame_url) {
  return createFrame(cross_frame_url)
    .then((frame) => receiveMessage())
    .then((message) => {
      if (message !== 'registration failed') {
        sendResultToTest('failed - ' + message);
        return;
      }
      sendResultToTest('ok - frame failed to register periodicSync');
    });
}

function registerPeriodicSyncFromServiceWorker(tag, minInterval) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      if (minInterval !== undefined) {
        swRegistration.active.postMessage(
          {action: 'registerPeriodicSync', tag: tag, minInterval: minInterval});
      } else {
        swRegistration.active.postMessage(
          {action: 'registerPeriodicSync', tag: tag});
      }

      sendResultToTest('ok - ' + tag + ' register sent to SW');
    })
    .catch(sendErrorToTest);
}

function hasOneShotSyncTag(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.getTags();
    })
    .then((tags) => {
      if (tags.indexOf(tag) >= 0) {
        sendResultToTest('ok - ' + tag + ' found');
      } else {
        sendResultToTest('error - ' + tag + ' not found');
        return;
      }
    })
    .catch(sendErrorToTest);
}

function hasPeriodicSyncTag(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.periodicSync.getTags();
    })
    .then((tags) => {
      if (tags.indexOf(tag) >= 0) {
        sendResultToTest('ok - ' + tag + ' found');
      } else {
        sendResultToTest('error - ' + tag + ' not found');
        return;
      }
    })
    .catch(sendErrorToTest);
}

function hasOneShotSyncTagFromServiceWorker(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'hasOneShotSyncTag', tag: tag});
      sendResultToTest('ok - hasTag sent to SW');
    })
    .catch(sendErrorToTest);
}

function hasPeriodicSyncTagFromServiceWorker(tag) {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'hasPeriodicSyncTag', tag: tag});
      sendResultToTest('ok - hasTag sent to SW');
    })
    .catch(sendErrorToTest);
}

function getOneShotSyncTags() {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.getTags();
    })
    .then((tags) => {
      sendResultToTest('ok - ' + tags.toString());
    })
    .catch(sendErrorToTest);
}

function getOneShotSyncTagsFromServiceWorker() {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({action: 'getOneShotSyncTags'});
      sendResultToTest('ok - getTags sent to SW');
    })
    .catch(sendErrorToTest);
}

function unregister(tag) {
  navigator.serviceWorker.ready
    .then(swRegistration => {
        return swRegistration.periodicSync.unregister(tag);
    })
    .then(() => {
      sendResultToTest('ok - ' + tag + ' unregistered');
    })
    .catch(sendErrorToTest);
}

function unregisterFromServiceWorker(tag) {
  navigator.serviceWorker.ready
    .then(swRegistration => {
      swRegistration.active.postMessage({action: 'unregister', tag: tag});
      sendResultToTest('ok - ' + tag + ' unregister sent to SW');
    })
    .catch(sendErrorToTest);
}

function completeDelayedSyncEvent() {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({
          action: 'completeDelayedSyncEvent'
        });
      sendResultToTest('ok - delay completing');
    })
    .catch(sendErrorToTest);
}

function rejectDelayedSyncEvent() {
  navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({action: 'rejectDelayedSyncEvent'});
      sendResultToTest('ok - delay rejecting');
    })
    .catch(sendErrorToTest);
}

function createFrame(url) {
  return new Promise((resolve) => {
    const frame = document.createElement('iframe');
    frame.src = url;
    frame.onload = () => { resolve(frame); };
    document.body.appendChild(frame);
  });
}

function receiveMessage() {
  return new Promise((resolve) => {
    window.addEventListener('message', (message) => {
      resolve(message.data);
    });
  });
}

function getNumPeriodicSyncEvents() {
  navigator.serviceWorker.ready
    .then(swRegistration => {
      swRegistration.active.postMessage({action: 'getPeriodicSyncEventCount'});
      sendResultToTest('ok - getting count of periodicsync events');
    })
    .catch(sendErrorToTest);
}

navigator.serviceWorker.addEventListener('message', (event) => {
  const message = event.data;
  const expected_messages = [
    'sync',
    'register',
    'unregister',
    'gotEventCount',
  ];

  if (expected_messages.includes(message.type))
    resultQueue.push(message.data);
}, false);
