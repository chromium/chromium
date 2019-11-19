// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
#define EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_

#include <string>

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ScriptContext;

// Provides the BindToGC native handler.
// TODO(devlin): Rename this from messaging bindings - it's no longer remotely
// messaging related.
class MessagingBindings : public ObjectBackedNativeHandler {
 public:
  explicit MessagingBindings(ScriptContext* script_context);
  ~MessagingBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void BindToGC(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(MessagingBindings);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
