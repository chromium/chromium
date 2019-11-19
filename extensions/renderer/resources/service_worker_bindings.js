// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This function is returned to WillEvaluateServiceWorkerOnWorkerThread
// then executed, passing in dependencies as function arguments.
//
// |backgroundUrl| is the URL of the extension's background page.
// |wakeEventPage| is a function that wakes up the current extension's event
// page, then runs its callback on completion or failure.
// |logging| is an object equivalent to a subset of base/debug/logging.h, with
// CHECK/DCHECK/etc.
(function(backgroundUrl, wakeEventPage, logging) {
  'use strict';
  self.chrome = self.chrome || {};
  self.chrome.runtime = self.chrome.runtime || {};

  // Returns a Promise that resolves to the background page's client, or null
  // if there is no background client.
  function findBackgroundClient() {
    return self.clients.matchAll({
      includeUncontrolled: true,
      type: 'window'
    }).then(function(clients) {
      return clients.find(function(client) {
        return client.url == backgroundUrl;
      });
    });
  }

  // Returns a Promise wrapper around wakeEventPage, that resolves on success,
  // or rejects on failure.
  function makeWakeEventPagePromise() {
    return new Promise(function(resolve, reject) {
      wakeEventPage(function(success) {
        if (success)
          resolve();
        else
          reject('Failed to start background client "' + backgroundUrl + '"');
      });
    });
  }

  // The chrome.runtime.getBackgroundClient function is documented in
  // runtime.json. It returns a Promise that resolves to the background page's
  // client, or is rejected if there is no background client or if the
  // background client failed to wake.
  self.chrome.runtime.getBackgroundClient = function() {
    return findBackgroundClient().then(function(client) {
      if (client) {
        // Background client is already awake, or it was persistent.
        return client;
      }

      // Event page needs to be woken.
      return makeWakeEventPagePromise().then(function() {
        return findBackgroundClient();
      }).then(function(client) {
        if (!client) {
          return Promise.reject(
            'Background client "' + backgroundUrl + '" not found');
        }
        return client;
      });
    });
  };
});
