// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_METRICS_H_
#define CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_METRICS_H_

namespace content {

// These values must stay in sync with tools/metrics/histograms.xml.
// Enum values should never be renumbered or reused as they are stored and can
// be used for multi-release queries.  Insert any new values before |kCount| and
// increment the count.
enum class KeyboardLockMethods {
  kRequestAllKeys = 0,
  kRequestSomeKeys = 1,
  kCancelLock = 2,
  kCount = 3
};

constexpr char kKeyboardLockMethodCalledHistogramName[] =
    "Blink.KeyboardLock.MethodCalled";

}  // namespace content

#endif  // CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_METRICS_H_
