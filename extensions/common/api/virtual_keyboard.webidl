// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <p>Determines whether advanced virtual keyboard features should be enabled
//     or not. They are enabled by default.</p>
// <p>On <b>Chrome 58</b> all properties are expected to have the same value.
// </p>
// <p>From <b>Chrome 63</b> the properties can be distinct and are optional.
// If omitted, the current value is preserved.</p>
dictionary FeatureRestrictions {
  // Whether virtual keyboards can provide auto-complete.
  boolean autoCompleteEnabled;
  // Whether virtual keyboards can provide auto-correct.
  boolean autoCorrectEnabled;
  // Whether virtual keyboards can provide input via handwriting recognition.
  boolean handwritingEnabled;
  // Whether virtual keyboards can provide spell-check.
  boolean spellCheckEnabled;
  // Whether virtual keyboards can provide voice input.
  boolean voiceInputEnabled;
};

// The <code>chrome.virtualKeyboard</code> API is a kiosk only API used to
// configure virtual keyboard layout and behavior in kiosk sessions.
[platforms=("chromeos")]
interface VirtualKeyboard {
  // Sets restrictions on features provided by the virtual keyboard.
  // |restrictions|: the preferences to enabled/disabled virtual keyboard
  // features.
  // |Returns|: Invoked with the values which were updated.
  // |PromiseValue|: update
  static Promise<FeatureRestrictions> restrictFeatures(
      FeatureRestrictions restrictions);
};

partial interface Browser {
  static attribute VirtualKeyboard virtualKeyboard;
};
