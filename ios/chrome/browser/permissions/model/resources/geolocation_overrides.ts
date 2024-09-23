// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

if (typeof navigator === 'object' && 'geolocation' in navigator) {
  if ('getCurrentPosition' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.getCurrentPosition;
    navigator.geolocation.getCurrentPosition = function() {
      sendWebKitMessage(
          'GeolocationAPIAccessedHandler', {'api': 'getCurrentPosition'});
      const originalArgs = arguments as
          unknown as [successCallback: PositionCallback,
                               errorCallback?: PositionErrorCallback|null|
                               undefined,
                               options?: PositionOptions|undefined];
      return originalFunc.apply(this, originalArgs);
    }
  }
  if ('watchPosition' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.watchPosition;
    navigator.geolocation.watchPosition = function() {
      sendWebKitMessage(
          'GeolocationAPIAccessedHandler', {'api': 'watchPosition'});
      const originalArgs = arguments as
          unknown as [successCallback: PositionCallback,
                               errorCallback?: PositionErrorCallback|null|
                               undefined,
                               options?: PositionOptions|undefined];
      return originalFunc.apply(this, originalArgs);
    }
  }
  if ('clearWatch' in navigator.geolocation) {
    const originalFunc = navigator.geolocation.clearWatch;
    navigator.geolocation.clearWatch = function() {
      sendWebKitMessage('GeolocationAPIAccessedHandler', {'api': 'clearWatch'});
      const originalArgs = arguments as unknown as [watchId: number];
      return originalFunc.apply(this, originalArgs);
    }
  }
}
