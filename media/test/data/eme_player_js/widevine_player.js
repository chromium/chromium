// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Widevine player responsible for playing media using Widevine key system.
function WidevinePlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

WidevinePlayer.prototype.init = function() {
  // Returns a promise.
  return PlayerUtils.initEMEPlayer(this);
};

WidevinePlayer.prototype.registerEventListeners = function() {
  // Returns a promise.
  return PlayerUtils.registerEMEEventListeners(this);
};

WidevinePlayer.prototype.onMessage = function(message) {
  var mediaKeySession = message.target;
  function onSuccess(response) {
    var key = new Uint8Array(response);
    Utils.timeLog('Update media key session with license response.', key);
    mediaKeySession.update(key)
        .catch(function(error) { Utils.failTest(error, EME_UPDATE_FAILED); });
  }
  Utils.sendRequest('POST',
                    'arraybuffer',
                    Utils.convertToUint8Array(message.message),
                    this.testConfig.licenseServerURL,
                    onSuccess,
                    this.testConfig.forceInvalidResponse);
};
