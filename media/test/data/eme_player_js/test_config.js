// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test configuration used by test page to configure the player app and other
// test specific configurations.
function TestConfig() {
  this.mediaFile = null;
  this.keySystem = null;
  this.mediaType = null;
  this.licenseServerURL = null;
  this.useMSE = false;
  this.runFPS = false;
  this.playCount = 0;
  this.configChangeType = CONFIG_CHANGE_TYPE.CLEAR_TO_CLEAR;
  this.policyCheck = false;
  this.MSESegmentDurationMS = 0;
  this.MSESegmentFetchDelayBeforeEndMS = 0;
}

TestConfig.prototype.loadQueryParams = function() {
  // Load query parameters and set default values.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  var decodeURI = function decodeURI(s) {
      return decodeURIComponent(s.replace(/\+/g, ' '));
  };
  var match;
  while (match = r.exec(window.location.search.substring(1)))
    this[decodeURI(match[1])] = decodeURI(match[2]);
  this.useMSE = this.useMSE == '1' || this.useMSE == 'true';
  this.playCount = parseInt(this.playCount) || 0;
  this.policyCheck = this.policyCheck == '1' || this.policyCheck == 'true';
  this.MSESegmentDurationMS = parseInt(this.MSESegmentDurationMS) || 0;
  this.MSESegmentFetchDelayBeforeEndMS =
    parseInt(this.MSESegmentFetchDelayBeforeEndMS) || 0;
};

TestConfig.updateDocument = function() {
  this.loadQueryParams();
  Utils.addOptions(KEYSYSTEM_ELEMENT_ID, KEY_SYSTEMS);
  Utils.addOptions(MEDIA_TYPE_ELEMENT_ID, MEDIA_TYPES);

  document.getElementById(MEDIA_FILE_ELEMENT_ID).value =
      this.mediaFile || DEFAULT_MEDIA_FILE;

  document.getElementById(LICENSE_SERVER_ELEMENT_ID).value =
      this.licenseServerURL || DEFAULT_LICENSE_SERVER;

  if (this.keySystem)
    Utils.ensureOptionInList(KEYSYSTEM_ELEMENT_ID, this.keySystem);
  if (this.mediaType)
    Utils.ensureOptionInList(MEDIA_TYPE_ELEMENT_ID, this.mediaType);
  document.getElementById(USE_MSE_ELEMENT_ID).value = this.useMSE;
  document.getElementById(USE_PLAY_COUNT_ELEMENT_ID).value = this.playCount;
};

TestConfig.init = function() {
  // Reload test configuration from document.
  this.mediaFile = document.getElementById(MEDIA_FILE_ELEMENT_ID).value;
  this.keySystem = document.getElementById(KEYSYSTEM_ELEMENT_ID).value;
  this.mediaType = document.getElementById(MEDIA_TYPE_ELEMENT_ID).value;
  this.useMSE = document.getElementById(USE_MSE_ELEMENT_ID).value == 'true';
  this.playCount = document.getElementById(USE_PLAY_COUNT_ELEMENT_ID).value;
  this.licenseServerURL =
      document.getElementById(LICENSE_SERVER_ELEMENT_ID).value;
};
