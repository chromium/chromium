// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_PROMISE_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_PROMISE_PROPERTIES_H_

// See ScriptPromiseProperty.h
#define SCRIPT_PROMISE_PROPERTIES(P, ...)       \
  P(ScriptPromise, kReady##__VA_ARGS__)         \
  P(ScriptPromise, kClosed##__VA_ARGS__)        \
  P(ScriptPromise, kFinished##__VA_ARGS__)      \
  P(ScriptPromise, kLoaded##__VA_ARGS__)        \
  P(ScriptPromise, kLost##__VA_ARGS__)          \
  P(ScriptPromise, kReleased##__VA_ARGS__)      \
  P(ScriptPromise, kResponseReady##__VA_ARGS__) \
  P(ScriptPromise, kUserChoice##__VA_ARGS__)    \
  P(ScriptPromise, kPreloadResponse##__VA_ARGS__)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_PROMISE_PROPERTIES_H_
