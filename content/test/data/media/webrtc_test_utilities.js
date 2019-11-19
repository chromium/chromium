// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These must match with how the video and canvas tags are declared in html.
const VIDEO_TAG_WIDTH = 320;
const VIDEO_TAG_HEIGHT = 240;

// Fake video capture background green is of value 135.
const COLOR_BACKGROUND_GREEN = 135;

var gPendingTimeout;

// Tells the C++ code we succeeded, which will generally exit the test.
function reportTestSuccess() {
  console.log('Test Success');
  window.domAutomationController.send('OK');
}

// Returns a custom return value to the test.
function sendValueToTest(value) {
  window.domAutomationController.send(value);
}

// Immediately fails the test on the C++ side.
function failTest(reason) {
  if (reason instanceof Error) {
    var error = reason;
  } else {
    var error = new Error(reason);
  }
  window.domAutomationController.send(error.stack);
}

// Fail a test on the C++ side after a timeout. Will cancel any pending timeout.
function failTestAfterTimeout(reason, timeout_ms) {
  cancelTestTimeout();
  gPendingTimeout = setTimeout(function() {
    failTest(reason);
  }, timeout_ms);
}

// Cancels the current test timeout.
function cancelTestTimeout() {
  clearTimeout(gPendingTimeout);
  gPendingTimeout = null;
}

function detectVideoPlaying(videoElementName) {
  return detectVideo(videoElementName, isVideoPlaying);
}

function detectVideoWithDimensionPlaying(
    videoElementName, video_width, video_height) {
  return detectVideoWithDimension(
      videoElementName, isVideoPlaying, video_width, video_height);
}

function detectVideoStopped(videoElementName) {
  return detectVideo(videoElementName, function(pixels, previous_pixels) {
    return !isVideoPlaying(pixels, previous_pixels);
  });
}

function detectBlackVideo(videoElementName) {
  return detectVideo(videoElementName, function(pixels, previous_pixels) {
    return isVideoBlack(pixels);
  });
}

function detectUniformColorVideoWithDimensionPlaying(
    videoElementName, video_width, video_height) {
  return detectVideoWithDimension(
      videoElementName, function(pixels, previous_pixels) {
        return isVideoPlaying(pixels, previous_pixels) &&
            arePixelsUniformColor(pixels) &&
            arePixelsUniformColor(previous_pixels);
      }, video_width, video_height);
}

function detectVideo(videoElementName, predicate) {
  return detectVideoWithDimension(
      videoElementName, predicate, VIDEO_TAG_WIDTH, VIDEO_TAG_HEIGHT);
}

function detectVideoWithDimension(
    videoElementName, predicate, video_width, video_height) {
  console.log('Looking at video in element ' + videoElementName);

  return new Promise((resolve, reject) => {
    var width = video_width;
    var height = video_height;
    var videoElement = $(videoElementName);
    var oldPixels = [];
    var startTimeMs = new Date().getTime();
    var waitVideo = setInterval(function() {
      var canvas = $(videoElementName + '-canvas');
      if (canvas == null) {
        console.log(
            'Waiting for ' + videoElementName + '-canvas' +
            ' to appear');
        return;
      }
      var context = canvas.getContext('2d');
      context.drawImage(videoElement, 0, 0);
      var pixels = context.getImageData(0, 0, width, height / 3).data;

      // Check that there is an old and a new picture with the same size to
      // compare and use the function |predicate| to detect the video state in
      // that case.
      // There's a failure(?) mode here where the video generated claims to
      // have size 2x2. Don't consider that a valid video.
      if (oldPixels.length == pixels.length && predicate(pixels, oldPixels)) {
        console.log('Done looking at video in element ' + videoElementName);
        console.log('DEBUG: video.width = ' + videoElement.videoWidth);
        console.log('DEBUG: video.height = ' + videoElement.videoHeight);
        clearInterval(waitVideo);
        resolve({
          'width': videoElement.videoWidth,
          'height': videoElement.videoHeight
        });
      }
      oldPixels = pixels;
      var elapsedTime = new Date().getTime() - startTimeMs;
      if (elapsedTime > 3000) {
        startTimeMs = new Date().getTime();
        console.log(
            'Still waiting for video to satisfy ' + predicate.toString());
        console.log('DEBUG: video.width = ' + videoElement.videoWidth);
        console.log('DEBUG: video.height = ' + videoElement.videoHeight);
      }
    }, 200);
  });
}

function waitForConnectionToStabilize(peerConnection) {
  return new Promise((resolve, reject) => {
    peerConnection.onsignalingstatechange =
        function(event) {
          if (peerConnection.signalingState == 'stable') {
            peerConnection.onsignalingstatechange = null;
            resolve();
          }
        }
  });
}

function waitForConnectionToStabilizeIfNeeded(peerConnection) {
  return new Promise((resolve, reject) => {
    if (peerConnection.signalingState == 'stable') {
      resolve();
      return;
    }
    return waitForConnectionToStabilize(peerConnection).then(resolve);
  });
}

// This very basic video verification algorithm will be satisfied if any
// pixels are changed.
function isVideoPlaying(pixels, previousPixels) {
  for (var i = 0; i < pixels.length; i++) {
    if (pixels[i] != previousPixels[i]) {
      return true;
    }
  }
  return false;
}

// Checks if the frame is black. |pixels| is in RGBA (i.e. pixels[0] is the R
// value for the first pixel).
function isVideoBlack(pixels) {
  var threshold = 20;
  var accumulatedLuma = 0;
  for (var i = 0; i < pixels.length; i += 4) {
    // Ignore the alpha channel.
    accumulatedLuma += rec702Luma_(pixels[i], pixels[i + 1], pixels[i + 2]);
    if (accumulatedLuma > threshold * (i / 4 + 1))
      return false;
  }
  return true;
}

// |pixels| is in RGBA (i.e. pixels[0] is the R value for the first pixel).
function arePixelsUniformColor(pixels) {
  if (pixels.length < 4) {
    failTest('expected at least one pixel');
  }
  var reference_r = pixels[0];
  var reference_g = pixels[1];
  var reference_b = pixels[2];
  var reference_a = pixels[3];
  for (var i = 4; i < pixels.length; i += 4) {
    if (pixels[i + 0] != reference_r) {
      console.log('red value at pixel ' + i + ' does not match reference');
      return false;
    }
    if (pixels[i + 1] != reference_g) {
      console.log('green value at pixel ' + i + ' does not match reference');
      return false;
    }
    if (pixels[i + 2] != reference_b) {
      console.log('blue value at pixel ' + i + ' does not match reference');
      return false;
    }
    if (pixels[i + 3] != reference_a) {
      console.log('alpha value at pixel ' + i + ' does not match reference');
      return false;
    }
  }
  return true;
}

// Checks if the given color is within 1 value away from COLOR_BACKGROUND_GREEN.
function isAlmostBackgroundGreen(color) {
  if (Math.abs(color - COLOR_BACKGROUND_GREEN) > 1)
    return false;
  return true;
}

// Use Luma as in Rec. 709: Y′709 = 0.2126R + 0.7152G + 0.0722B;
// See http://en.wikipedia.org/wiki/Rec._709.
function rec702Luma_(r, g, b) {
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

// This function matches |left| and |right| and fails the test if the
// values don't match using normal javascript equality (i.e. the hard
// types of the operands aren't checked).
function assertEquals(expected, actual) {
  if (actual != expected) {
    failTest('expected \'' + expected + '\', got \'' + actual + '\'.');
  }
}

function assertNotEquals(expected, actual) {
  if (actual === expected) {
    failTest('expected \'' + expected + '\', got \'' + actual + '\'.');
  }
}

function assertTrue(booleanExpression, description) {
  if (!booleanExpression) {
    failTest(description);
  }
}

function assertFalse(booleanExpression, description) {
  if (!!booleanExpression) {
    failTest(description);
  }
}
