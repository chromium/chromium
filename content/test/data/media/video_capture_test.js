// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$ = function(id) {
  return document.getElementById(id);
};

var hasReceivedTrackEndedEvent = false;
var hasReceivedDeviceChangeEventReceived = false;

async function startVideoCaptureAndVerifySize(video_width, video_height) {
  console.log('Calling getUserMediaAndWaitForVideoRendering.');
  var constraints = {
    video: {
      width: {exact: video_width},
      height: {exact: video_height},
    }
  };
  let stream;
  try {
    stream = await navigator.mediaDevices.getUserMedia(constraints);
  } catch (err) {
    throw getUserMediaError(err);
  }
  return waitForVideoStreamToSatisfyRequirementFunction(
      stream, detectVideoWithDimensionPlaying, video_width, video_height);
}

async function startVideoCaptureFromVirtualDeviceAndVerifyUniformColorVideoWithSize(
    video_width, video_height) {
  console.log('Trying to find device named "Virtual Device".');
  const devices = await navigator.mediaDevices.enumerateDevices();
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
    throw new Error(
        'No video input device was found with label = Virtual ' +
        'Device');
  }
  var device_specific_constraints = {
    video: {
      width: {exact: video_width},
      height: {exact: video_height},
      deviceId: {exact: target_device.deviceId}
    }
  };
  let stream;
  try {
    stream =
      await navigator.mediaDevices.getUserMedia(device_specific_constraints);
  } catch (err) {
    throw getUserMediaError(err);
  }
  return waitForVideoStreamToSatisfyRequirementFunction(
            stream, detectUniformColorVideoWithDimensionPlaying, video_width,
            video_height);
}

function enumerateVideoCaptureDevicesAndVerifyCount(expected_count) {
  console.log('Enumerating devices and verifying count.');
  return navigator.mediaDevices.enumerateDevices().then(function(devices) {
    var actual_count = 0;
    devices.forEach(function(device) {
      if (device.kind == 'videoinput') {
        console.log('Found videoinput device with label ' + device.label);
        actual_count = actual_count + 1;
      }
    });
    if (actual_count == expected_count) {
      return logSuccess();
    } else {
      throw new Error(
          'Device count ' + actual_count + ' did not match expectation of ' +
          expected_count);
    }
  });
}

function getUserMediaError(error) {
  return new Error('GetUserMedia call failed with code ' + error.code);
}

async function waitForVideoStreamToSatisfyRequirementFunction(
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
    throw new Error('Did not receive any video tracks');
  }
  var videoTrack = videoTracks[0];
  videoTrack.onended = function() {
    hasReceivedTrackEndedEvent = true;
  };

  await requirementFunction('local-view', video_width, video_height);
  if (localView.videoWidth == video_width) {
    return logSuccess();
  } else {
    throw new Error('Video has unexpected width.');
  }
}

async function waitForVideoToTurnBlack() {
  await detectBlackVideo('local-view');
  return logSuccess();
}

function verifyHasReceivedTrackEndedEvent() {
  if (hasReceivedTrackEndedEvent) {
    return logSuccess();
  } else {
    throw new Error('Did not receive ended event from track.');
  }
}

function registerForDeviceChangeEvent() {
  navigator.mediaDevices.ondevicechange = function(event) {
    hasReceivedDeviceChangeEventReceived = true;
  };
}

async function waitForDeviceChangeEvent() {
  while (!hasReceivedDeviceChangeEventReceived) {
    console.log('still waiting for device change event');
    await new Promise(resolve => setTimeout(resolve, 200));
  }
  return logSuccess();
}

function resetHasReceivedChangedEventFlag() {
  hasReceivedDeviceChangeEventReceived = false;
}
