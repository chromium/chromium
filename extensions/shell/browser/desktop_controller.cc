// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/desktop_controller.h"

#include <ostream>

#include "base/check.h"
#include "base/macros.h"

namespace extensions {
namespace {

DesktopController* g_instance = NULL;

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
  g_instance = NULL;
}

}  // namespace extensions
