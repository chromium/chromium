// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/utility/utility_thread.h"


namespace content {

namespace {

// Keep the global UtilityThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
constinit thread_local UtilityThread* utility_thread = nullptr;

}  // namespace

UtilityThread* UtilityThread::Get() {
  return utility_thread;
}

UtilityThread::UtilityThread() : resetter_(&utility_thread, this) {}

UtilityThread::~UtilityThread() = default;

}  // namespace content

