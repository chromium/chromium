// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

if (typeof navigator === 'object' && 'geolocation' in navigator) {
  if ('getCurrentPosition' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.getCurrentPosition;
    navigator.geolocation.getCurrentPosition = function(...args) {
      sendWebKitMessage(
          'GeolocationAPIAccessedHandler', {'api': 'getCurrentPosition'});
      return originalFunc.apply(this, args);
    }
  }
  if ('watchPosition' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.watchPosition;
    navigator.geolocation.watchPosition = function(...args) {
      sendWebKitMessage(
          'GeolocationAPIAccessedHandler', {'api': 'watchPosition'});
      return originalFunc.apply(this, args);
    }
  }
  if ('clearWatch' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.clearWatch;
    navigator.geolocation.clearWatch = function(...args) {
      sendWebKitMessage('GeolocationAPIAccessedHandler', {'api': 'clearWatch'});
      return originalFunc.apply(this, args);
    }
  }
}
