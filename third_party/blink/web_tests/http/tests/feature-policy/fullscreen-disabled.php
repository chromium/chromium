<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that fullscreen feature when disabled may not be called by
// any iframe even when allowfullscreen is set.

Header("Feature-Policy: fullscreen 'none'");
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
      assert_false(data.enabled, 'Document.fullscreenEnabled:');
      assert_equals(data.type, 'error', 'Document.requestFullscreen():');
    });
  }, 'Fullscreen disabled on URL: ' + src + ' with allowfullscreen = ' +
    allowfullscreen);
}

window.onload = function() {
  loadIframes(srcs);
}
</script>
