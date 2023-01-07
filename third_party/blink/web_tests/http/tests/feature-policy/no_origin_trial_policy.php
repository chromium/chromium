<?php
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

header("Feature-Policy: frobulate *");
?>
<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
test(function() {
  assert_false(document.featurePolicy.allowsFeature("frobulate"));
}, 'Test featurePolicy.allowsFeature() on unavailable origin trial feature');

test(function() {
  assert_false(document.featurePolicy.allowsFeature("frobulate", "https://www.example.com"));
}, 'Test featurePolicy.allowsFeature() on unavailable origin trial feature for specific origin');

test(function() {
  assert_false(document.featurePolicy.features().includes("frobulate"));
}, 'Test featurePolicy.features() should not include origin trial feature');

test(function() {
  assert_false(document.featurePolicy.allowedFeatures().includes("frobulate"));
}, 'Test featurePolicy.allowedFeatures() should not include origin trial feature');

test(function() {
  assert_array_equals(
    document.featurePolicy.getAllowlistForFeature("frobulate"), [])
}, "Origin trial features should not have an allowlist.");

</script>
