// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager_observer.h"

namespace extensions {

// Ensure invalidated weak pointers are not left inside the observer list.
// See crbug.com/417539782.
ProcessManagerObserver::~ProcessManagerObserver() {
  CHECK(!IsInObserverList());
}

}  // namespace extensions
