// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_producer.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_common.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

namespace blink::v8_compile_hints {

V8LocalCompileHintsProducer::V8LocalCompileHintsProducer() {
  static bool enabled =
      base::FeatureList::IsEnabled(features::kLocalCompileHints);
  should_generate_data_ = enabled;
}

void V8LocalCompileHintsProducer::RecordScript(
    LocalFrame* frame,
    ExecutionContext* execution_context,
    const v8::Local<v8::Script> script,
    ClassicScript* classic_script) {
  if (!should_generate_data_) {
    return;
  }
  if (classic_script->CacheHandler() == nullptr) {
    return;
  }
  v8::Isolate* isolate = execution_context->GetIsolate();
  v8_scripts_.emplace_back(v8::Global<v8::Script>(isolate, script));
  classic_scripts_.emplace_back(Persistent<ClassicScript>(classic_script));
}

void V8LocalCompileHintsProducer::GenerateData(LocalFrame* frame) {
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window);
  ExecutionContext* execution_context = window->GetExecutionContext();
  v8::Isolate* isolate = execution_context->GetIsolate();
  CodeCacheHost* code_cache_host =
      ExecutionContext::GetCodeCacheHostFromContext(execution_context);
  v8::HandleScope handle_scope(isolate);

  DCHECK_EQ(classic_scripts_.size(), v8_scripts_.size());
  for (wtf_size_t i = 0; i < classic_scripts_.size(); ++i) {
    const ClassicScript& classic_script = *classic_scripts_.at(i);
    CachedMetadataHandler* cache_handler = classic_script.CacheHandler();
    CHECK(cache_handler);

    v8::Local<v8::Script> script = v8_scripts_[i].Get(isolate);
    std::vector<int> compile_hints = script->GetProducedCompileHints();
    if (compile_hints.size() == 0) {
      continue;
    }

    uint64_t timestamp = V8CodeCache::GetTimestamp();
    std::unique_ptr<v8::ScriptCompiler::CachedData> data(
        CreateCompileHintsCachedDataForScript(compile_hints, timestamp));

    cache_handler->ClearCachedMetadata(code_cache_host,
                                       CachedMetadataHandler::kClearLocally);
    cache_handler->SetCachedMetadata(
        code_cache_host, V8CodeCache::TagForCompileHints(cache_handler),
        data->data, data->length);
  }
}

v8::ScriptCompiler::CachedData*
V8LocalCompileHintsProducer::CreateCompileHintsCachedDataForScript(
    std::vector<int>& compile_hints,
    uint64_t prefix) {
  std::sort(compile_hints.begin(), compile_hints.end());

  size_t hints_count = compile_hints.size();
  constexpr size_t prefix_size = sizeof(uint64_t);
  size_t data_size = hints_count * sizeof(int) + prefix_size;
  std::unique_ptr<uint8_t[]> data(new uint8_t[data_size]);

  // Add the prefix in a little-endian manner.
  size_t ix = 0;
  for (size_t i = 0; i < prefix_size; ++i) {
    data[ix++] = prefix & 0xff;
    prefix >>= 8;
  }

  for (size_t j = 0; j < hints_count; ++j) {
    // Add every int in a little-endian manner.
    int hint = compile_hints[j];
    for (size_t k = 0; k < sizeof(int); ++k) {
      data[ix++] = hint & 0xff;
      hint >>= 8;
    }
  }
  DCHECK_EQ(data_size, ix);

  return new v8::ScriptCompiler::CachedData(
      data.release(), static_cast<int>(data_size),
      v8::ScriptCompiler::CachedData::BufferOwned);
}

void V8LocalCompileHintsProducer::Trace(Visitor* visitor) const {
  visitor->Trace(classic_scripts_);
}

}  // namespace blink::v8_compile_hints
