// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// EMEApp is responsible for starting playback on the eme_player.html page.
// It selects the suitable player based on key system and other test options.
function EMEApp(testConfig) {
  this.video_ = null;
  this.testConfig_ = testConfig;
  this.updateDocument(testConfig);
}

EMEApp.prototype.createPlayer = function() {
  // Load document test configuration.
  this.updateTestConfig();
  if (this.video_) {
    Utils.timeLog('Delete old video tag.');
    this.video_.pause();
    this.video_.remove();
    delete(this.video_);
  }

  this.video_ = document.createElement('video');
  this.video_.controls = true;
  this.video_.preload = true;
  this.video_.width = 848;
  this.video_.height = 480;
  var videoSpan = document.getElementById(VIDEO_ELEMENT_ID);
  if (videoSpan)
    videoSpan.appendChild(this.video_);
  else
    document.body.appendChild(this.video_);

  var videoPlayer = PlayerUtils.createPlayer(this.video_, this.testConfig_);
  if (!videoPlayer) {
    Utils.timeLog('Cannot create a media player.');
    return Promise.reject('Cannot create a media player.');
  }

  Utils.timeLog('Using ' + videoPlayer.constructor.name);
  if (this.testConfig_.runFPS)
    FPSObserver.observe(this.video_);

  return videoPlayer.init().then(function(result) {
    if (result != videoPlayer) {
      Utils.timeLog('Media player mismatch.');
    }
    Utils.timeLog('Media player created.');
    return videoPlayer;
  });
};

EMEApp.prototype.updateDocument = function(testConfig) {
  // Update document lists with test configuration values.
  Utils.addOptions(KEYSYSTEM_ELEMENT_ID, KEY_SYSTEMS);
  Utils.addOptions(MEDIA_TYPE_ELEMENT_ID, MEDIA_TYPES);
  document.getElementById(MEDIA_FILE_ELEMENT_ID).value =
      testConfig.mediaFile || DEFAULT_MEDIA_FILE;
  document.getElementById(LICENSE_SERVER_ELEMENT_ID).value =
      testConfig.licenseServerURL || DEFAULT_LICENSE_SERVER;
  if (testConfig.keySystem)
    Utils.ensureOptionInList(KEYSYSTEM_ELEMENT_ID, testConfig.keySystem);
  if (testConfig.mediaType)
    Utils.ensureOptionInList(MEDIA_TYPE_ELEMENT_ID, testConfig.mediaType);
  document.getElementById(USE_MSE_ELEMENT_ID).value = testConfig.useMSE;
};

EMEApp.prototype.updateTestConfig = function() {
  // Reload test configuration from document.
  this.testConfig_.mediaFile =
      document.getElementById(MEDIA_FILE_ELEMENT_ID).value;
  this.testConfig_.keySystem =
      document.getElementById(KEYSYSTEM_ELEMENT_ID).value;
  this.testConfig_.mediaType =
      document.getElementById(MEDIA_TYPE_ELEMENT_ID).value;
  this.testConfig_.useMSE =
      document.getElementById(USE_MSE_ELEMENT_ID).value == 'true';
  this.testConfig_.licenseServerURL =
      document.getElementById(LICENSE_SERVER_ELEMENT_ID).value;
};
