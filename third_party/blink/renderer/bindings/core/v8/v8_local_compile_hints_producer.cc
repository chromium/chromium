// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_producer.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_common.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"

namespace blink::v8_compile_hints {

V8LocalCompileHintsProducer::V8LocalCompileHintsProducer(LocalFrame* frame)
    : frame_(frame) {
  should_generate_data_ = frame->IsMainFrame() &&
      base::FeatureList::IsEnabled(features::kLocalCompileHints);
}

void V8LocalCompileHintsProducer::RecordScript(
    ExecutionContext* execution_context,
    const v8::Local<v8::Script> script,
    ClassicScript* classic_script) {
  if (!should_generate_data_) {
    return;
  }
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
  if (cache_handler == nullptr) {
    return;
  }
  v8::Isolate* isolate = execution_context->GetIsolate();
  compile_hints_collectors_.emplace_back(isolate,
                                         script->GetCompileHintsCollector());
  cache_handlers_.emplace_back(cache_handler);
}

void V8LocalCompileHintsProducer::GenerateData(bool final_data) {
  DCHECK_EQ(cache_handlers_.size(), compile_hints_collectors_.size());
  if (cache_handlers_.empty()) {
    return;
  }

  LocalDOMWindow* window = frame_->DomWindow();
  CHECK(window);
  ExecutionContext* execution_context = window->GetExecutionContext();
  v8::Isolate* isolate = execution_context->GetIsolate();
  CodeCacheHost* code_cache_host =
      ExecutionContext::GetCodeCacheHostFromContext(execution_context);
  v8::HandleScope handle_scope(isolate);

  for (wtf_size_t i = 0; i < cache_handlers_.size(); ++i) {
    CachedMetadataHandler* cache_handler = cache_handlers_.at(i);

    if (V8CodeCache::HasCodeCache(cache_handler,
                                  CachedMetadataHandler::kAllowUnchecked)) {
      // We're trying to set compile hints even though the code cache exists
      // already. This can happen if the user navigated around on the website
      // and the script became so hot that a code cache was created.
      base::UmaHistogramBoolean(kLocalCompileHintsObsoletedByCodeCacheHistogram,
                                true);
      return;
    }
    base::UmaHistogramBoolean(kLocalCompileHintsObsoletedByCodeCacheHistogram,
                              false);

    V8CodeCache::RecordCacheSetStatistics(
        final_data
            ? V8CodeCache::SetMetadataType::kLocalCompileHintsAtInteractive
            : V8CodeCache::SetMetadataType::kLocalCompileHintsAtFMP);

    v8::Local<v8::CompileHintsCollector> compile_hints_collector =
        compile_hints_collectors_.at(i).Get(isolate);
    std::vector<int> compile_hints =
        compile_hints_collector->GetCompileHints(isolate);

    uint64_t timestamp = V8CodeCache::GetTimestamp();
    std::unique_ptr<v8::ScriptCompiler::CachedData> data(
        CreateCompileHintsCachedDataForScript(compile_hints, timestamp));

    cache_handler->ClearCachedMetadata(code_cache_host,
                                       CachedMetadataHandler::kClearLocally);
    cache_handler->SetCachedMetadata(
        code_cache_host, V8CodeCache::TagForCompileHints(cache_handler),
        data->data, data->length);
  }
  if (final_data) {
    cache_handlers_.clear();
    compile_hints_collectors_.clear();
    base::UmaHistogramEnumeration(kLocalCompileHintsGeneratedHistogram,
                                  LocalCompileHintsGenerated::kFinal);

  } else {
    base::UmaHistogramEnumeration(kLocalCompileHintsGeneratedHistogram,
                                  LocalCompileHintsGenerated::kNonFinal);
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
  auto data = base::HeapArray<uint8_t>::Uninit(data_size);

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
      std::move(data).leak().data(), static_cast<int>(data_size),
      v8::ScriptCompiler::CachedData::BufferOwned);
}

void V8LocalCompileHintsProducer::Trace(Visitor* visitor) const {
  visitor->Trace(cache_handlers_);
  visitor->Trace(frame_);
  visitor->Trace(compile_hints_collectors_);
}

}  // namespace blink::v8_compile_hints
