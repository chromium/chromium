// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$ = function(id) {
  return document.getElementById(id);
};

var hasReceivedTrackEndedEvent = false;
var hasReceivedDeviceChangeEventReceived = false;

function startVideoCaptureAndVerifySize(video_width, video_height) {
  console.log('Calling getUserMediaAndWaitForVideoRendering.');
  var constraints = {
    video: {
      width: {exact: video_width},
      height: {exact: video_height},
    }
  };
  navigator.mediaDevices.getUserMedia(constraints)
      .then(function(stream) {
        waitForVideoStreamToSatisfyRequirementFunction(
            stream, detectVideoWithDimensionPlaying, video_width, video_height);
      })
      .catch(failedCallback);
}

function startVideoCaptureFromVirtualDeviceAndVerifyUniformColorVideoWithSize(
    video_width, video_height) {
  console.log('Trying to find device named "Virtual Device".');
  navigator.mediaDevices.enumerateDevices().then(function(devices) {
    var target_device;
    devices.forEach(function(device) {
      if (device.kind == 'videoinput') {
        console.log('Found videoinput device with label ' + device.label);
        if (device.label == 'Virtual Device') {
          target_device = device;
        }
      }
    });
    if (target_device == null) {
      failTest(
          'No video input device was found with label = Virtual ' +
          'Device');
      return;
    }
    var device_specific_constraints = {
      video: {
        width: {exact: video_width},
        height: {exact: video_height},
        deviceId: {exact: target_device.deviceId}
      }
    };
    navigator.mediaDevices.getUserMedia(device_specific_constraints)
        .then(function(stream) {
          waitForVideoStreamToSatisfyRequirementFunction(
              stream, detectUniformColorVideoWithDimensionPlaying, video_width,
              video_height);
        })
        .catch(failedCallback);
  });
}

function enumerateVideoCaptureDevicesAndVerifyCount(expected_count) {
  console.log('Enumerating devices and verifying count.');
  navigator.mediaDevices.enumerateDevices().then(function(devices) {
    var actual_count = 0;
    devices.forEach(function(device) {
      if (device.kind == 'videoinput') {
        console.log('Found videoinput device with label ' + device.label);
        actual_count = actual_count + 1;
      }
    });
    if (actual_count == expected_count) {
      reportTestSuccess();
    } else {
      failTest(
          'Device count ' + actual_count + ' did not match expectation of ' +
          expected_count);
    }
  });
}

function failedCallback(error) {
  failTest('GetUserMedia call failed with code ' + error.code);
}

function waitForVideoStreamToSatisfyRequirementFunction(
    stream, requirementFunction, video_width, video_height) {
  var localView = $('local-view');
  localView.width = video_width;
  localView.height = video_height;
  localView.srcObject = stream;

  var canvas = $('local-view-canvas');
  canvas.width = video_width;
  canvas.height = video_height;

  var videoTracks = stream.getVideoTracks();
  if (videoTracks.length == 0) {
    failTest('Did not receive any video tracks');
  }
  var videoTrack = videoTracks[0];
  videoTrack.onended = function() {
    hasReceivedTrackEndedEvent = true;
  };

  requirementFunction('local-view', video_width, video_height).then(() => {
    if (localView.videoWidth == video_width) {
      reportTestSuccess();
    } else {
      failTest('Video has unexpected width.');
    }
  });
}

function waitForVideoToTurnBlack() {
  detectBlackVideo('local-view').then(() => {
    reportTestSuccess();
  });
}

function verifyHasReceivedTrackEndedEvent() {
  if (hasReceivedTrackEndedEvent) {
    reportTestSuccess();
  } else {
    failTest('Did not receive ended event from track.');
  }
}

function registerForDeviceChangeEvent() {
  navigator.mediaDevices.ondevicechange = function(event) {
    hasReceivedDeviceChangeEventReceived = true;
  };
}

function waitForDeviceChangeEvent() {
  if (hasReceivedDeviceChangeEventReceived) {
    reportTestSuccess();
  } else {
    console.log('still waiting for device change event');
    setTimeout(waitForDeviceChangeEvent, 200);
  }
}

function resetHasReceivedChangedEventFlag() {
  hasReceivedDeviceChangeEventReceived = false;
}
