// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_WASM_MODULE_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_WASM_MODULE_SCRIPT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class KURL;
class Modulator;
class ModuleScriptCreationParams;

// WasmModuleScript is a model object for the "WebAssembly module script"
// spec concept. https://html.spec.whatwg.org/C/#webassembly-module-script
class CORE_EXPORT WasmModuleScript final : public ModuleScript,
                                           public NameClient {
 public:
  // https://html.spec.whatwg.org/C/#creating-a-webassembly-module-script
  static WasmModuleScript* Create(
      const ModuleScriptCreationParams& params,
      Modulator*,
      const ScriptFetchOptions&,
      const TextPosition& start_position = TextPosition::MinimumPosition());

  const char* GetHumanReadableName() const override {
    return "WasmModuleScript";
  }

  WasmModuleScript(Modulator* settings_object,
                   v8::Local<v8::WasmModuleObject> wasm_module,
                   const KURL& source_url,
                   const KURL& base_url,
                   const ScriptFetchOptions& fetch_options,
                   const TextPosition& start_position);

  v8::Local<v8::Module> V8Module() const override { NOTREACHED(); }
  BoxedV8Module* BoxModuleRecord() const override;
  v8::Local<v8::WasmModuleObject> WasmModule() const override;
  bool IsWasmModuleRecord() const override { return true; }
  Vector<ModuleRequest> GetModuleRecordRequests() const override {
    return Vector<ModuleRequest>();
  }
  ScriptValue Instantiate() const override;

  static v8::Local<v8::WasmModuleObject> EmptyModuleForTesting(v8::Isolate*);

 private:
  friend class ModuleMapTestModulator;

  // This byte sequence corresponds to an empty WebAssembly module with only
  // the magic bytes and version number provided.
  static constexpr uint8_t kEmptyWasmByteSequenceRaw[] = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  static constexpr base::span<const uint8_t, 8> kEmptyWasmByteSequence =
      base::span<const uint8_t, 8>(kEmptyWasmByteSequenceRaw);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_WASM_MODULE_SCRIPT_H_
