// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dispatcher_delegate.h"

#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

void DispatcherDelegate::RequireWebViewModules(ScriptContext* context) {
  // If the embedder does not define its own WebView, we'll provide a default
  // implementation.
  context->module_system()->Require("extensionsWebViewElement");
}

}  // namespace extensions
