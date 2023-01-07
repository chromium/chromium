<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test tests that the JavaScript exposure of feature policy in the main
// document works via the following methods:
//     allowsFeature(feature)
//         -- if |feature| is allowed on the origin of the document.
//     allowsFeature(feature, origin)
//         -- if |feature| is allowed on the given origin.
//     allowedFeatures()
//         -- a list of features that are enabled on the origin of the
//            document.
//     getAllowlistForFeatureForFeature(feature)
//         -- a list of explicitly named origins where the given feature is
//            enabled, or
//            ['*'] if the feature is enabled on all origins.

Header("Feature-Policy: fullscreen *; payment 'self'; midi 'none'; camera 'self' https://www.example.com https://www.example.net");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
var policy_main = document.featurePolicy;
var allowed_features = ["fullscreen", "payment", "camera"];
var disallowed_features = ["badfeature", "midi"];

// Tests for featurePolicy.allowsFeature().
for (var feature of allowed_features) {
  test(function() {
    assert_true(policy_main.allowsFeature(feature));
    assert_true(policy_main.allowsFeature(feature, "http://127.0.0.1:8000"));
  }, 'Test featurePolicy.allowsFeature() on feature ' + feature);
}

test(function() {
  assert_true(policy_main.allowsFeature("camera", "https://www.example.com"));
  assert_true(policy_main.allowsFeature("camera", "https://www.example.net"));
}, 'Test featurePolicy.allowsFeature() for camera');

for (var feature of disallowed_features) {
  test(function() {
    assert_false(policy_main.allowsFeature(feature));
    assert_false(policy_main.allowsFeature(feature, "http://127.0.0.1:8000"));
  }, 'Test featurePolicy.allowsFeature() on disallowed feature ' + feature);
}

// Tests for featurePolicy.allowedFeatures().
var allowed_features_main = policy_main.allowedFeatures();
for (var feature of allowed_features) {
  test(function() {
    assert_true(allowed_features_main.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() include feature ' + feature);
}
for (var feature of disallowed_features) {
  test(function() {
    assert_false(allowed_features_main.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() does not include disallowed feature ' +
    feature);
}

// Tests for featurePolicy.getAllowlistForFeature().
assert_array_equals(
  policy_main.getAllowlistForFeature("fullscreen"), ["*"],
  "fullscreen is allowed for all in main frame");
assert_array_equals(
  policy_main.getAllowlistForFeature("payment"), ["http://127.0.0.1:8000"],
  "payment is allowed for self");
assert_array_equals(
  policy_main.getAllowlistForFeature("camera").sort(),
  ["http://127.0.0.1:8000",
   "https://www.example.com",
   "https://www.example.net"].sort(),
  "camera is allowed for multiple origins");
assert_array_equals(
  policy_main.getAllowlistForFeature("midi"), [], "midi is disallowed for all");
</script>
