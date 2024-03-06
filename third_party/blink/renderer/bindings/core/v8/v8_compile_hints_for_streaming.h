// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_FOR_STREAMING_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_FOR_STREAMING_H_

#include <memory>

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

class CachedMetadata;
class KURL;

namespace v8_compile_hints {

class V8CrowdsourcedCompileHintsProducer;
class V8LocalCompileHintsConsumer;

// An utility class for using the CompileHints feature with
// v8::ScriptCompiler::StartStreaming. This class takes care of managing both
// crowdsourced and local compile hints. This can be created by calling
// Builder::Build() method. The Builder object can be created on the main
// thread and its Build() method can be called on non-main thread where we
// receive the CachedMetadata from the network service when the
// BackgroundResourceFetch feature is enabled.
class CORE_EXPORT CompileHintsForStreaming {
 public:
  class CORE_EXPORT Builder {
   public:
    Builder(
        V8CrowdsourcedCompileHintsProducer* crowdsourced_compile_hints_producer,
        V8CrowdsourcedCompileHintsConsumer* crowdsourced_compile_hints_consumer,
        const KURL& resource_url);
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder() = default;

    // This method can be called on non-main thread.
    std::unique_ptr<CompileHintsForStreaming> Build(
        scoped_refptr<CachedMetadata> cached_metadata) &&;

   private:
    const bool might_generate_crowdsourced_compile_hints_;
    std::unique_ptr<V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>
        crowdsourced_compile_hint_callback_data_;
  };

  // For producing compile hints.
  explicit CompileHintsForStreaming(base::PassKey<Builder>);
  // For consuming local compile hints.
  CompileHintsForStreaming(
      std::unique_ptr<V8LocalCompileHintsConsumer> local_compile_hints_consumer,
      base::PassKey<Builder>);
  // For consuming crowdsourced compile hints.
  CompileHintsForStreaming(
      std::unique_ptr<V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>
          crowdsourced_compile_hint_callback_data,
      base::PassKey<Builder>);

  CompileHintsForStreaming(const CompileHintsForStreaming&) = delete;
  CompileHintsForStreaming& operator=(const CompileHintsForStreaming&) = delete;

  ~CompileHintsForStreaming() = default;

  v8::ScriptCompiler::CompileOptions compile_options() const {
    return compile_options_;
  }
  v8::CompileHintCallback GetCompileHintCallback() const;
  void* GetCompileHintCallbackData() const;

  V8LocalCompileHintsConsumer* GetV8LocalCompileHintsConsumerForTest() const;

 private:
  const v8::ScriptCompiler::CompileOptions compile_options_;
  const std::unique_ptr<V8LocalCompileHintsConsumer>
      local_compile_hints_consumer_;
  const std::unique_ptr<
      V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>
      crowdsourced_compile_hint_callback_data_;
};

}  // namespace v8_compile_hints
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_FOR_STREAMING_H_
