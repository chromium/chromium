<?php
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that composited animations are still interpolated even when
// the 'layout-animations' feature is disabled.

Header("Feature-Policy: layout-animations 'none'");
?>
<!DOCTYPE html>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<style>
#box {
  position: relative;
  height: 100px;
  width: 100px;
  background-color: blue;
}

@keyframes animation {
  from {
    transform: translateX(100px);
  }
  to {
    transform: translateX(0);
  }
}

#box.target {
  animation-duration: 1s;
  animation-name: animation;
  animation-timing-function: linear;
}
</style>
<div id="box"></div>
<script>
async_test(function(t) {
  requestAnimationFrame(function () { validate(t) });
}, "Check that allowed animated properties are interpolated even when others " +
   "are disabled by policy");

function validate(t) {
  var box = document.getElementById('box');

  assert_equals(getComputedStyle(box).width, '100px', 'initial width');

  // Start the animation.
  box.className = 'target';
  getComputedStyle(box).width;  // Force the animation.

  assert_equals(box.getAnimations().length, 1, 'animation started');
  var animation = box.getAnimations()[0];

  animation.currentTime = 0;
  assert_equals(getComputedStyle(box).transform, 'matrix(1, 0, 0, 1, 100, 0)', 'width at animation start');
  animation.currentTime = 490;
  assert_equals(getComputedStyle(box).transform, 'matrix(1, 0, 0, 1, 51, 0)', 'width at animation start');
  animation.currentTime = 510;
  assert_equals(getComputedStyle(box).transform, 'matrix(1, 0, 0, 1, 49, 0)', 'width at animation start');
  animation.currentTime = 999;
  assert_equals(getComputedStyle(box).transform, 'matrix(1, 0, 0, 1, 0.1, 0)', 'width at animation start');
  animation.currentTime = 1000;

  assert_equals(box.getAnimations().length, 0, 'animation ended');
  t.done();
}
</script>
