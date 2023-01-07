// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClearKeyPlayer responsible for playing media using Clear Key key system.
function ClearKeyPlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

ClearKeyPlayer.prototype.init = function() {
  // Returns a promise.
  return PlayerUtils.initEMEPlayer(this);
};

ClearKeyPlayer.prototype.registerEventListeners = function() {
  // Returns a promise.
  return PlayerUtils.registerEMEEventListeners(this);
};

ClearKeyPlayer.prototype.onMessage = function(message) {
  const mediaKeySession = message.target;
  const keyId = Utils.extractFirstLicenseKeyId(message.message);
  const key = Utils.getDefaultKey(this.testConfig.forceInvalidResponse);
  const jwkSet = Utils.createJWKData(keyId, key);
  const keySystem = this.testConfig.keySystem;

  // Number of milliseconds in 100 years, which is approximately
  // 100 * 365 * 24 * 60 * 60 * 1000.
  // See clear_key_cdm.cc where this value is set.
  const ECK_RENEWAL_EXPIRATION = 3153600000000;

  Utils.timeLog('Calling update: ' + String.fromCharCode.apply(null, jwkSet));
  mediaKeySession.update(jwkSet).then(function() {
    // Check session expiration.
    // - For CLEARKEY, expiration is not set and is the default value NaN.
    // - For MESSAGE_TYPE_TEST_KEYSYSTEM, expiration is set to
    //   ECK_RENEWAL_EXPIRATION milliseconds after 01 January 1970 UTC.
    // - For other EXTERNAL_CLEARKEY variants, expiration is explicitly set to
    //   NaN.
    var expiration = mediaKeySession.expiration;
    if (keySystem == MESSAGE_TYPE_TEST_KEYSYSTEM) {
      if (isNaN(expiration) || expiration != ECK_RENEWAL_EXPIRATION) {
        Utils.timeLog('Unexpected expiration: ', expiration);
        Utils.failTest(error, EME_UPDATE_FAILED);
      }
    } else {
      if (!isNaN(mediaKeySession.expiration)) {
        Utils.timeLog('Unexpected expiration: ', expiration);
        Utils.failTest(error, EME_UPDATE_FAILED);
      }
    }
  }).catch(function(error) {
    // Ignore the error if a crash is expected. This ensures that the decoder
    // actually detects and reports the error.
    if (this.testConfig.keySystem != CRASH_TEST_KEYSYSTEM) {
      Utils.failTest(error, EME_UPDATE_FAILED);
    }
  });
};
