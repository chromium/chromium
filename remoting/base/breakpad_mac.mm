// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace remoting {

void InitializeCrashReporting() {
  // Not implemented, see https://crbug.com/714714
}

}  // namespace remoting
