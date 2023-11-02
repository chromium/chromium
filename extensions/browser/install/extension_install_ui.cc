// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install/extension_install_ui.h"

namespace extensions {

// static
bool ExtensionInstallUI::disable_ui_for_tests_ = false;

ExtensionInstallUI::ExtensionInstallUI() {
}

ExtensionInstallUI::~ExtensionInstallUI() {
}

}  // namespace extensions
