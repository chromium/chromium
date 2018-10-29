<?php
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that non-composited animations are not interpolated when
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
  opacity: 0.8;
  background-color: blue;
}

@keyframes animation {
  from {
    width: 100px;
    opacity: 0.8;;
  }
  to {
    width: 200px;
    opacity: 0.5;
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
}, "Check that animated properties disabled by policy are non-interpolated");

function validate(t) {
  var box = document.getElementById('box');

  assert_equals(getComputedStyle(box).width, '100px', 'initial width');

  // Start the animation.
  box.className = 'target';
  getComputedStyle(box).width;  // Force the animation.

  assert_equals(box.getAnimations().length, 1, 'animation started');
  var animation = box.getAnimations()[0];

  animation.currentTime = 0;
  assert_equals(getComputedStyle(box).width, '100px', 'width at animation start');
  animation.currentTime = 490;
  assert_equals(getComputedStyle(box).width, '100px', 'width just before midpoint');
  animation.currentTime = 510;
  assert_equals(getComputedStyle(box).width, '200px', 'width just after midpoint');
  animation.currentTime = 999;
  assert_equals(getComputedStyle(box).width, '200px', 'width at end');
  animation.currentTime = 1000;

  assert_equals(box.getAnimations().length, 0, 'animation ended');
  t.done();
}
</script>
