// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"

namespace extensions {

ExtensionOptionsGuestDelegate::ExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest)
    : guest_(guest) {
}

ExtensionOptionsGuestDelegate::~ExtensionOptionsGuestDelegate() {
}

}  // namespace extensions
