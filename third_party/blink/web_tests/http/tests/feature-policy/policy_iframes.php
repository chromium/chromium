<?php
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test tests that the JavaScript exposure of feature policy in iframes
// works via the following methods:
//     allowsFeature(feature)
//         -- if |feature| is allowed on the src origin of the iframe.
//     allowsFeature(feature, origin)
//         -- if |feature| is allowed on the given origin in the iframe.
//     allowedFeatures()
//         -- a list of features that are enabled on the src origin of the
//            iframe.
//     getAllowlistForFeatureForFeature(feature)
//         -- a list of explicitly named origins where the given feature is
//            enabled, or
//            ['*'] if the feature is enabled on all origins.

Header("Feature-Policy: fullscreen *; payment 'self'; midi 'none'; camera 'self' http://www.example.com https://www.example.net http://localhost:8000");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<iframe id="f1" src="../resources/dummy.html"></iframe>
<iframe id="f2" src="http://localhost:8000/../resources/dummy.html"></iframe>
<script>
var local_iframe_policy = document.getElementById("f1").featurePolicy;
var remote_iframe_policy = document.getElementById("f2").featurePolicy;
var local_src = "http://127.0.0.1:8000";
var remote_src = "http://localhost:8000";
// Tests for featurePolicy.allowsFeature().
// Without an allow attribute, 'fullscreen' should be only be allowed in
// same-origin frames, for its own origin.
test(function() {
  assert_true(local_iframe_policy.allowsFeature("fullscreen"));
  assert_true(local_iframe_policy.allowsFeature("fullscreen", local_src));
  assert_false(local_iframe_policy.allowsFeature("fullscreen", remote_src));
  assert_false(remote_iframe_policy.allowsFeature("fullscreen"));
  assert_false(remote_iframe_policy.allowsFeature("fullscreen", remote_src));
  assert_false(remote_iframe_policy.allowsFeature("fullscreen", local_src));
  assert_false(local_iframe_policy.allowsFeature("fullscreen", "http://www.example.com"));
  assert_false(remote_iframe_policy.allowsFeature("fullscreen", "http://www.example.com"));
}, 'Test featurePolicy.allowsFeature() on fullscreen');

// Camera should be allowed in both iframes on src origin but no
test(function() {
  assert_true(local_iframe_policy.allowsFeature("camera"));
  assert_true(local_iframe_policy.allowsFeature("camera", local_src));
  assert_false(local_iframe_policy.allowsFeature("camera", remote_src));
  assert_false(remote_iframe_policy.allowsFeature("camera"));
  assert_false(remote_iframe_policy.allowsFeature("camera", remote_src));
  assert_false(remote_iframe_policy.allowsFeature("camera", local_src));
  assert_false(local_iframe_policy.allowsFeature("camera", "http://www.example.com"));
  assert_false(remote_iframe_policy.allowsFeature("camera", "http://www.example.com"));
}, 'Test featurePolicy.allowsFeature() on camera');
// payment should only be allowed in the local iframe on src origin:
test(function() {
  assert_true(local_iframe_policy.allowsFeature("payment"));
  assert_true(local_iframe_policy.allowsFeature("payment", local_src));
  assert_false(local_iframe_policy.allowsFeature("payment", remote_src));
  assert_false(remote_iframe_policy.allowsFeature("payment"));
  assert_false(remote_iframe_policy.allowsFeature("payment", local_src));
  assert_false(remote_iframe_policy.allowsFeature("payment", remote_src));
}, 'Test featurePolicy.allowsFeature() on locally allowed feature payment');
// badfeature and midi should be disallowed in both iframes:
for (var feature of ["badfeature", "midi"]) {
  test(function() {
    assert_false(local_iframe_policy.allowsFeature(feature));
    assert_false(local_iframe_policy.allowsFeature(feature, local_src));
    assert_false(local_iframe_policy.allowsFeature(feature, remote_src));
    assert_false(remote_iframe_policy.allowsFeature(feature));
    assert_false(remote_iframe_policy.allowsFeature(feature, local_src));
    assert_false(remote_iframe_policy.allowsFeature(feature, remote_src));
  }, 'Test featurePolicy.allowsFeature() on disallowed feature ' + feature);
}

// Tests for featurePolicy.allowedFeatures().
var allowed_local_iframe_features = local_iframe_policy.allowedFeatures();
var allowed_remote_iframe_features = remote_iframe_policy.allowedFeatures();
for (var feature of ["fullscreen", "camera", "payment", "geolocation"]) {
  test(function() {
    assert_true(allowed_local_iframe_features.includes(feature));
    assert_false(allowed_remote_iframe_features.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() locally include feature ' + feature +
  '  but not remotely ');
}
for (var feature of ["badfeature", "midi"]) {
  test(function() {
    assert_false(allowed_local_iframe_features.includes(feature));
    assert_false(allowed_remote_iframe_features.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() does not include disallowed feature ' +
    feature);
}

// Dynamically change policies: Allow as much as possible with attribute
document.getElementById("f1").allow = "fullscreen *; camera *; payment *; geolocation *; midi *";
document.getElementById("f2").allow = "fullscreen *; camera *; payment *; geolocation *; midi *";

// Tests for featurePolicy.allowedFeatures().
var allowed_local_iframe_features = local_iframe_policy.allowedFeatures();
var allowed_remote_iframe_features = remote_iframe_policy.allowedFeatures();
for (var feature of ["fullscreen", "camera", "geolocation"]) {
  test(function() {
    assert_true(allowed_local_iframe_features.includes(feature));
    assert_true(allowed_remote_iframe_features.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() include feature ' + feature + ' with allow *');
}
for (var feature of ["badfeature", "midi"]) {
  test(function() {
    assert_false(allowed_local_iframe_features.includes(feature));
    assert_false(allowed_remote_iframe_features.includes(feature));
  }, 'Test featurePolicy.allowedFeatures() does not include disallowed feature ' +
    feature + ' with allow *');
}
for (var feature of ["payment"]) {
test(function() {
  assert_true(allowed_local_iframe_features.includes(feature));
  assert_false(allowed_remote_iframe_features.includes(feature));
}, 'Test featurePolicy.allowedFeatures() locally include feature ' + feature +
  '  but not remotely with allow *');
}

// Tests for featurePolicy.getAllowlistForFeature().
test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("fullscreen"), [local_src]);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("fullscreen"), [remote_src]);
}, 'featurePolicy.getAllowlistForFeature(): fullscreen is allowed in both iframes');
test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("payment"), [local_src]);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("payment"), []);
}, 'featurePolicy.getAllowlistForFeature(): payment is allowed only in local iframe');
test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("geolocation"), [local_src]);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("geolocation"), [remote_src]);
}, 'featurePolicy.getAllowlistForFeature(): geolocation is allowed only in local iframe');
test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("midi"), []);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("midi"), []);
}, 'featurePolicy.getAllowlistForFeature(): midi is disallowed in both iframe');

// Dynamically update iframe policy: Restrict with allow attribute.
document.getElementById("f1").allow = "fullscreen 'none'; payment 'src'; midi 'src'; geolocation 'none'; camera 'src' 'self' https://www.example.com https://www.example.net";
document.getElementById("f2").allow = "fullscreen 'none'; payment 'src'; midi 'src'; geolocation 'none'; camera 'src' 'self' https://www.example.com https://www.example.net";
test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("fullscreen"), []);
  assert_array_equals(
    document.getElementById("f1").featurePolicy.getAllowlistForFeature("fullscreen"),
    []);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("fullscreen"), []);
  assert_array_equals(
    document.getElementById("f2").featurePolicy.getAllowlistForFeature("fullscreen"),
    []);
}, 'Dynamically redefine allow: fullscreen is disallowed in both iframes');

test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("payment"), [local_src]);
  assert_array_equals(
    document.getElementById("f1").featurePolicy.getAllowlistForFeature("payment"),
    [local_src]);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("payment"), []);
  assert_array_equals(
    document.getElementById("f2").featurePolicy.getAllowlistForFeature("payment"),
    []);
}, 'Dynamically redefine allow: payment is allowed in local frame only');

test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("geolocation"), []);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("geolocation"), []);
}, 'Dynamically redefine allow: geolocation is disallowed in both iframes');

test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("camera"), [local_src]);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("camera"), [remote_src]);
}, 'Dynamically redefine allow: camera is allowed in both iframes');

test(function() {
  assert_array_equals(
    local_iframe_policy.getAllowlistForFeature("midi"), []);
  assert_array_equals(
    remote_iframe_policy.getAllowlistForFeature("midi"), []);
}, 'Dynamically redefine allow: midi is still disallowed in both iframe');
</script>
