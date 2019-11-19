// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Intentionally leaked during fuzzing.
// See testing/libfuzzer/efficient_fuzzer.md.
DummyPageHolder* g_page_holder = nullptr;
WebBlobInfoArray* g_blob_info_array = nullptr;

enum : uint32_t {
  kFuzzMessagePorts = 1 << 0,
  kFuzzBlobInfo = 1 << 1,
};

}  // namespace

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  const char kExposeGC[] = "--expose_gc";
  v8::V8::SetFlagsFromString(kExposeGC, sizeof(kExposeGC));
  static BlinkFuzzerTestSupport fuzzer_support =
      BlinkFuzzerTestSupport(*argc, *argv);
  g_page_holder = std::make_unique<DummyPageHolder>().release();
  g_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  g_blob_info_array = new WebBlobInfoArray();
  g_blob_info_array->emplace_back(WebBlobInfo::BlobForTesting(
      "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "text/plain", 12));
  g_blob_info_array->emplace_back(
      WebBlobInfo::FileForTesting("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                                  "/native/path", "path", "text/plain"));
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) {
  // Odd sizes are handled in various ways, depending how they arrive.
  // Let's not worry about that case here.
  if (data_size % sizeof(UChar))
    return 0;

  // Truncate the input.
  wtf_size_t size = base::saturated_cast<wtf_size_t>(data_size);

  // Used to control what kind of extra data is provided to the deserializer.
  unsigned hash = StringHasher::HashMemory(data, size);

  SerializedScriptValue::DeserializeOptions options;

  // If message ports are requested, make some.
  if (hash & kFuzzMessagePorts) {
    MessagePortArray* message_ports = MakeGarbageCollected<MessagePortArray>(3);
    std::generate(message_ports->begin(), message_ports->end(), []() {
      auto* port =
          MakeGarbageCollected<MessagePort>(g_page_holder->GetDocument());
      port->Entangle(mojo::MessagePipe().handle0);
      return port;
    });
    options.message_ports = message_ports;
  }

  // If blobs are requested, supply blob info.
  options.blob_info = (hash & kFuzzBlobInfo) ? g_blob_info_array : nullptr;

  // Set up.
  ScriptState* script_state =
      ToScriptStateForMainWorld(&g_page_holder->GetFrame());
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(isolate);

  // Deserialize.
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Create(reinterpret_cast<const char*>(data), size);
  serialized_script_value->Deserialize(isolate, options);
  CHECK(!try_catch.HasCaught())
      << "deserialize() should return null rather than throwing an exception.";

  // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
  //
  // Multiple GCs may be required to ensure everything is collected (due to
  // a chain of persistent handles), so some objects may not be collected until
  // a subsequent iteration. This is slow enough as is, so we compromise on one
  // major GC, as opposed to the 5 used in V8GCController for unit tests.
  V8PerIsolateData::MainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  return 0;
}

}  // namespace blink

// Explicitly specify some attributes to avoid issues with the linker dead-
// stripping the following function on macOS, as it is not called directly
// by fuzz target. LibFuzzer runtime uses dlsym() to resolve that function.
#if defined(OS_MACOSX)
__attribute__((used)) __attribute__((visibility("default")))
#endif  // defined(OS_MACOSX)
extern "C" int
LLVMFuzzerInitialize(int* argc, char*** argv) {
  return blink::LLVMFuzzerInitialize(argc, argv);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
