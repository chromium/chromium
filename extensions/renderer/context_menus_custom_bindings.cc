// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/context_menus_custom_bindings.h"

#include <stdint.h>

#include <atomic>

#include "base/bind.h"
#include "base/callback.h"
#include "v8/include/v8.h"

namespace {

std::atomic<int32_t> menu_counter{0};

void GetNextContextMenuId(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(crbug.com/942373): We should use base::UnguessableToken or base::GUID
  // here, and move to using a string for all context menu IDs.
  args.GetReturnValue().Set(++menu_counter);
}

}  // namespace

namespace extensions {

ContextMenusCustomBindings::ContextMenusCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void ContextMenusCustomBindings::AddRoutes() {
  RouteHandlerFunction("GetNextContextMenuId",
                       base::BindRepeating(&GetNextContextMenuId));
}

}  // extensions
