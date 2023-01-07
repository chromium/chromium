<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that fullscreen feature when enabled for all works across
// origins regardless of whether allowfullscreen is set. (Feature policy header
// takes precedence over the absence of allowfullscreen.)

Header("Feature-Policy: fullscreen *");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script src="resources/helper.js"></script>
<iframe></iframe>
<iframe allowfullscreen></iframe>
<script>
var srcs = [
  "resources/feature-policy-fullscreen.html",
  "http://localhost:8000/feature-policy/resources/feature-policy-fullscreen.html"
];

function loadFrame(iframe, src) {
  var allowfullscreen = iframe.allowFullscreen;
  promise_test(function() {
    iframe.src = src;
    return new Promise(function(resolve, reject) {
      window.addEventListener('message', function(e) {
        resolve(e.data);
      }, { once: true });
    }).then(function(data) {
      // fullscreen is enabled if:
      //     a. same origin; or
      //     b. enabled by allowfullscreen.
      if (src === srcs[0] || allowfullscreen) {
        assert_true(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'change', 'Document.requestFullscreen():');
      } else {
        assert_false(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'error', 'Document.requestFullscreen():');
      }
    });
  }, 'Fullscreen enabled for all on URL: ' + src + ' with allowfullscreen = ' +
    allowfullscreen);
}

window.onload = function() {
  loadIframes(srcs);
}
</script>
