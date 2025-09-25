// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum PanDirection {
  "stop",
  "right",
  "left"
};

enum TiltDirection {
  "stop",
  "up",
  "down"
};

enum Protocol {
  "visca"
};

enum AutofocusState {
  "on",
  "off"
};

dictionary ProtocolConfiguration {
  Protocol protocol;
};

dictionary WebcamConfiguration {
  double pan;
  double panSpeed;
  PanDirection panDirection;
  double tilt;
  double tiltSpeed;
  TiltDirection tiltDirection;
  double zoom;
  AutofocusState autofocusState;
  double focus;
};

dictionary Range {
  required double min;
  required double max;
};

dictionary WebcamCurrentConfiguration {
  required double pan;
  required double tilt;
  required double zoom;
  required double focus;

  // Supported range of pan, tilt and zoom values.
  Range panRange;
  Range tiltRange;
  Range zoomRange;
  Range focusRange;
};

// Webcam Private API.
interface WebcamPrivate {
  // Open a serial port that controls a webcam.
  // |PromiseValue|: webcamId
  [requiredCallback] static Promise<DOMString> openSerialWebcam(
      DOMString path,
      ProtocolConfiguration protocol);

  // Close a serial port connection to a webcam.
  static undefined closeWebcam(DOMString webcamId);

  // Retrieve webcam parameters. Will respond with a config holding the
  // requested values that are available, or default values for those that
  // aren't. If none of the requests succeed, will respond with an error.
  // |PromiseValue|: configuration
  [requiredCallback] static Promise<WebcamCurrentConfiguration> get(
      DOMString webcamId);

  // A callback is included here which is invoked when the function responds.
  // No configuration is returned through it.
  // |PromiseValue|: configuration
  [requiredCallback] static Promise<WebcamCurrentConfiguration> set(
      DOMString webcamId,
      WebcamConfiguration config);

  // Reset a webcam. Note: the value of the parameter have no effect, it's the
  // presence of the parameter that matters. E.g.: reset(webcamId, {pan: 0,
  // tilt: 1}); will reset pan & tilt, but not zoom.
  // A callback is included here which is invoked when the function responds.
  // No configuration is returned through it.
  // |PromiseValue|: configuration
  [requiredCallback] static Promise<WebcamCurrentConfiguration> reset(
      DOMString webcamId,
      WebcamConfiguration config);

  // Set home preset for a webcam. A callback is included here which is
  // invoked when the function responds.
  // |PromiseValue|: configuration
  [requiredCallback] static Promise<WebcamCurrentConfiguration> setHome(
      DOMString webcamId);

  // Restore the camera's position to that of the specified preset. A callback
  // is included here which is invoked when the function responds.
  // |PromiseValue|: configuration
  [requiredCallback]
  static Promise<WebcamCurrentConfiguration> restoreCameraPreset(
      DOMString webcamId,
      double presetNumber);

  // Set the current camera's position to be stored for the specified preset.
  // A callback is included here which is invoked when the function responds.
  // |PromiseValue|: configuration
  [requiredCallback] static Promise<WebcamCurrentConfiguration> setCameraPreset(
      DOMString webcamId,
      double presetNumber);
};

partial interface Browser {
  static attribute WebcamPrivate webcamPrivate;
};
