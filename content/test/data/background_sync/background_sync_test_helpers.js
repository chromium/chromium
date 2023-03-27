// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const resultQueue = new ResultQueue();

function registerServiceWorker() {
  return navigator.serviceWorker.register('service_worker.js', {scope: './'})
    .then(() => {
      return navigator.serviceWorker.ready;
    })
    .then((swRegistration) => {
      return 'ok - service worker registered';
    })
    .catch(formatError);
}

function registerOneShotSync(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.register(tag);
    })
    .then(() => {
      return 'ok - ' + tag + ' registered';
    })
    .catch(formatError);
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
      return 'ok - iframe registered sync';
    })
    .catch(formatError);
}

function registerOneShotSyncFromCrossOriginFrame(cross_frame_url) {
  return createFrame(cross_frame_url)
    .then((frame) => {
      return receiveMessage();
    })
    .then((message) => {
      console.log(message);
      if (message !== 'registration failed') {
        return 'failed - ' + message;
      }
      return 'ok - frame failed to register sync';
    });
}

function registerOneShotSyncFromServiceWorker(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'registerOneShotSync', tag: tag});
      return 'ok - ' + tag + ' register sent to SW';
    })
    .catch(formatError);
}

function registerPeriodicSync(tag, minInterval) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      if (minInterval !== undefined) {
        return swRegistration.periodicSync.register(
            tag, {minInterval: minInterval});
      } else {
        return swRegistration.periodicSync.register(tag);
      }
    })
    .then(() => {
      return 'ok - ' + tag + ' registered';
    })
    .catch(formatError);
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
      return 'ok - iframe registered periodicSync';
    })
    .catch(formatError);
}

function registerPeriodicSyncFromCrossOriginFrame(cross_frame_url) {
  return createFrame(cross_frame_url)
    .then((frame) => receiveMessage())
    .then((message) => {
      if (message !== 'registration failed') {
        return 'failed - ' + message;
      }
      return 'ok - frame failed to register periodicSync';
    });
}

function registerPeriodicSyncFromServiceWorker(tag, minInterval) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      if (minInterval !== undefined) {
        swRegistration.active.postMessage(
          {action: 'registerPeriodicSync', tag: tag, minInterval: minInterval});
      } else {
        swRegistration.active.postMessage(
          {action: 'registerPeriodicSync', tag: tag});
      }

      return 'ok - ' + tag + ' register sent to SW';
    })
    .catch(formatError);
}

function hasOneShotSyncTag(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.getTags();
    })
    .then((tags) => {
      if (tags.indexOf(tag) >= 0) {
        return 'ok - ' + tag + ' found';
      } else {
        return 'error - ' + tag + ' not found';
      }
    })
    .catch(formatError);
}

function hasPeriodicSyncTag(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.periodicSync.getTags();
    })
    .then((tags) => {
      if (tags.indexOf(tag) >= 0) {
        return 'ok - ' + tag + ' found';
      } else {
        return 'error - ' + tag + ' not found';
      }
    })
    .catch(formatError);
}

function hasOneShotSyncTagFromServiceWorker(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'hasOneShotSyncTag', tag: tag});
      return 'ok - hasTag sent to SW';
    })
    .catch(formatError);
}

function hasPeriodicSyncTagFromServiceWorker(tag) {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage(
          {action: 'hasPeriodicSyncTag', tag: tag});
      return 'ok - hasTag sent to SW';
    })
    .catch(formatError);
}

function getOneShotSyncTags() {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      return swRegistration.sync.getTags();
    })
    .then((tags) => {
      return 'ok - ' + tags.toString();
    })
    .catch(formatError);
}

function getOneShotSyncTagsFromServiceWorker() {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({action: 'getOneShotSyncTags'});
      return 'ok - getTags sent to SW';
    })
    .catch(formatError);
}

function unregister(tag) {
  return navigator.serviceWorker.ready
    .then(swRegistration => {
        return swRegistration.periodicSync.unregister(tag);
    })
    .then(() => {
      return 'ok - ' + tag + ' unregistered';
    })
    .catch(formatError);
}

function unregisterFromServiceWorker(tag) {
  return navigator.serviceWorker.ready
    .then(swRegistration => {
      swRegistration.active.postMessage({action: 'unregister', tag: tag});
      return 'ok - ' + tag + ' unregister sent to SW';
    })
    .catch(formatError);
}

function completeDelayedSyncEvent() {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({
          action: 'completeDelayedSyncEvent'
        });
      return ('ok - delay completing');
    })
    .catch(formatError);
}

function rejectDelayedSyncEvent() {
  return navigator.serviceWorker.ready
    .then((swRegistration) => {
      swRegistration.active.postMessage({action: 'rejectDelayedSyncEvent'});
      return 'ok - delay rejecting';
    })
    .catch(formatError);
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
  return navigator.serviceWorker.ready
    .then(swRegistration => {
      swRegistration.active.postMessage({action: 'getPeriodicSyncEventCount'});
      return 'ok - getting count of periodicsync events';
    })
    .catch(formatError);
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
