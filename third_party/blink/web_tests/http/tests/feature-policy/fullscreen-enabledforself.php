<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that fullscreen feature when enabled for self only works
// in the same orgin but not cross origins when allowfullscreen is set. No
// iframe may call it when allowfullscreen is not set.

Header("Feature-Policy: fullscreen 'self'");
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
      // fullscreen is enabled if same origin, and blocked if not, regardless of
      // the allowfullscreen attribute.
      if (src === srcs[0]) {
        assert_true(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'change', 'Document.requestFullscreen():');
      } else {
        assert_false(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'error', 'Document.requestFullscreen():');
      }
    });
  }, 'Fullscreen enabled for self on URL: ' + src + ' with allowfullscreen = ' +
    allowfullscreen);
}

window.onload = function() {
  loadIframes(srcs);
}
</script>
