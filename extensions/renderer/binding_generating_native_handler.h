// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDING_GENERATING_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_BINDING_GENERATING_NATIVE_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "extensions/renderer/native_handler.h"

namespace extensions {

class ScriptContext;

// Generates API bindings based on the JSON/IDL schemas. This is done by
// creating a |Binding| (from binding.js) for the schema and generating the
// bindings from that.
class BindingGeneratingNativeHandler : public NativeHandler {
 public:
  // Generates binding for |api_name|, and sets the |bind_to| property on the
  // Object returned by |NewInstance| to the generated binding.
  BindingGeneratingNativeHandler(ScriptContext* context,
                                 const std::string& api_name,
                                 const std::string& bind_to);

  void Initialize() final;
  bool IsInitialized() final;
  v8::Local<v8::Object> NewInstance() override;

 private:
  ScriptContext* context_;
  std::string api_name_;
  std::string bind_to_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDING_GENERATING_NATIVE_HANDLER_H_
