// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_PROCESS_INFO_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_PROCESS_INFO_NATIVE_HANDLER_H_

#include "extensions/common/extension_id.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

class ScriptContext;

// TODO(devlin): This now only provides the extension ID for the context; is
// there another native handler it would make sense to wrap this in?
class ProcessInfoNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ProcessInfoNativeHandler(ScriptContext* context);
  ~ProcessInfoNativeHandler() override;
  ProcessInfoNativeHandler(const ProcessInfoNativeHandler&) = delete;
  ProcessInfoNativeHandler& operator=(const ProcessInfoNativeHandler&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  ExtensionId extension_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_PROCESS_INFO_NATIVE_HANDLER_H_
