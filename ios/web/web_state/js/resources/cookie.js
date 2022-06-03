// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Add functionality related to cookies.
 */
goog.provide('__crWeb.cookie');

//Requires __crWeb.message

/* Beginning of anonymous namespace. */
(function() {

function isCrossOriginFrame() {
  try {
    return (!window.top.location.hostname);
  } catch (e) {
    return true;
  }
}

function cookiesAllowed(state) {
  switch (state) {
    case 'enabled':
      return true;
    case 'block-third-party':
      return !isCrossOriginFrame();
    case 'block':
      return false;
    default:
      return true;
  }
}

var state = '$(COOKIE_STATE)';

if (cookiesAllowed(state)) {
  return;
}

var brokenOverrides = [];

function addOverride(object, descriptorObject, propertyName, getter, setter) {
  var original =
      Object.getOwnPropertyDescriptor(descriptorObject, propertyName);
  if (original && original.configurable) {
    var propertyDescriptor = {
      get: getter,
      configurable: false,
    };
    if (setter) {
      propertyDescriptor.set = setter;
    }
    Object.defineProperty(object, propertyName, propertyDescriptor);
  } else {
    brokenOverrides.push(propertyName);
  }
}

addOverride(
    document, Document.prototype, 'cookie',
    function() {
      return '';
    },
    function(value) {
      // no-op;
    });

addOverride(window, window, 'localStorage', function() {
  throw new DOMException(
      'Failed to read the \'localStorage\' property from \'window\': ' +
          'Access is denied for this document',
      'SecurityError');
}, null);

addOverride(window, window, 'sessionStorage', function() {
  throw new DOMException(
      'Failed to read the \'sessionStorage\' property from \'window\': ' +
          'Access is denied for this document',
      'SecurityError');
}, null);

// Caches are only supported in a SecureContext. Only add the override if
// caches are supported.
if (window.caches) {
  const promiseException = new DOMException(
      'An attempt was made to break through the security policy of the user ' +
          'agent.',
      'SecurityError');
  addOverride(caches, CacheStorage.prototype, 'match', function() {
    return function(request, options) {
      return Promise.reject(promiseException);
    }
  });

  addOverride(caches, CacheStorage.prototype, 'has', function() {
    return function(cacheName) {
      return Promise.reject(promiseException);
    }
  });

  addOverride(caches, CacheStorage.prototype, 'open', function() {
    return function(cacheName) {
      return Promise.reject(promiseException);
    }
  });

  addOverride(caches, CacheStorage.prototype, 'delete', function() {
    return function(cacheName) {
      return Promise.reject(promiseException);
    }
  });

  addOverride(caches, CacheStorage.prototype, 'keys', function() {
    return function() {
      return Promise.reject(promiseException);
    }
  });
}

// Safari automatically disables indexedDB in cross-origin iframes. Leave the
// default property there so users see a better error message.
if (!isCrossOriginFrame()) {
  if (!(delete window.indexedDB)) {
    brokenOverrides.push('indexedDB');
  }
}

if (brokenOverrides.length > 0) {
  __gCrWeb.message.invokeOnHost(
      {'command': 'cookie.error', 'brokenOverrides': brokenOverrides});
}
}());
