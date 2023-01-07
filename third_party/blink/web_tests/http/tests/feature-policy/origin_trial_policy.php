<?php
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

header("Feature-Policy: frobulate *");

// TODO(iclelland): Generate this sample token during the build. The token
// below will expire in 2033, but it would be better to always have a token which
// is guaranteed to be valid when the tests are run.
// Generate this token with the command:
// generate_token.py http://127.0.0.1:8000 Frobulate -expire-timestamp=2000000000
header("Origin-Trial: AlCoOPbezqtrGMzSzbLQC4c+oPqO6yuioemcBPjgcXajF8jtmZr4B8tJRPAARPbsX6hDeVyXCKHzEJfpBXvZgQEAAABReyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
?>
<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
test(function() {
  assert_true(document.featurePolicy.allowsFeature("frobulate"));
}, 'Test featurePolicy.allowsFeature() on origin trial feature');

test(function() {
  assert_true(document.featurePolicy.allowsFeature("frobulate", "https://www.example.com"));
}, 'Test featurePolicy.allowsFeature() on origin trial feature for specific origin');

test(function() {
  assert_true(document.featurePolicy.features().includes("frobulate"));
}, 'Test featurePolicy.features() includes origin trial feature');

test(function() {
  assert_true(document.featurePolicy.allowedFeatures().includes("frobulate"));
}, 'Test featurePolicy.allowedFeatures() includes origin trial feature');

test(function() {
  assert_array_equals(
    document.featurePolicy.getAllowlistForFeature("frobulate"), ["*"])
}, "Origin trial feature is allowed for all in main frame");

</script>
