// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This forwarding header allows to import the file using import directives
// assuming the code is build as a framework bundle while building it as a
// source set. This is required as multiple third-party dependencies disagree on
// that point.
#import "ios/third_party/motion_animator_objc/src/src/MotionAnimator.h"
