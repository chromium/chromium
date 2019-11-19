// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPLAY_SOURCE_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_DISPLAY_SOURCE_CUSTOM_BINDINGS_H_

#include <memory>

#include "base/macros.h"
#include "extensions/common/api/display_source.h"
#include "extensions/renderer/api/display_source/display_source_session.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Implements custom bindings for the displaySource API.
class DisplaySourceCustomBindings : public ObjectBackedNativeHandler {
 public:
  DisplaySourceCustomBindings(ScriptContext* context,
                              NativeExtensionBindingsSystem* bindings_system);
  ~DisplaySourceCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  //  ObjectBackedNativeHandler override.
  void Invalidate() override;

  void StartSession(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void TerminateSession(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  // Call completion callbacks.
  void OnCallCompleted(int call_id,
                       bool success,
                       const std::string& error_message);
  void OnSessionStarted(int sink_id,
                        int call_id,
                        bool success,
                        const std::string& error_message);
  // Dispatch events
  void DispatchSessionTerminated(int sink_id) const;
  void DispatchSessionError(int sink_id,
                            DisplaySourceErrorType type,
                            const std::string& message) const;

  // DisplaySession notification callbacks.
  void OnSessionTerminated(int sink_id);
  void OnSessionError(int sink_id,
                      DisplaySourceErrorType type,
                      const std::string& message);

  DisplaySourceSession* GetDisplaySession(int sink_id) const;

  std::map<int, std::unique_ptr<DisplaySourceSession>> session_map_;

  NativeExtensionBindingsSystem* bindings_system_;

  base::WeakPtrFactory<DisplaySourceCustomBindings> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplaySourceCustomBindings);
};

}  // extensions

#endif  // EXTENSIONS_RENDERER_DISPLAY_SOURCE_CUSTOM_BINDINGS_H_
