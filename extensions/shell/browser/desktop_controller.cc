// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/desktop_controller.h"

#include <ostream>

#include "base/check.h"

namespace extensions {
namespace {

DesktopController* g_instance = nullptr;

}  // namespace

// static
DesktopController* DesktopController::instance() {
  return g_instance;
}

DesktopController::DesktopController() {
  CHECK(!g_instance) << "DesktopController already exists";
  g_instance = this;
}

DesktopController::~DesktopController() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

}  // namespace extensions
