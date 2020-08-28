// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class FrameAssociatedMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  using GetV8MemoryUsageCallback =
      mojom::blink::V8DetailedMemoryReporter::GetV8MemoryUsageCallback;

  explicit FrameAssociatedMeasurementDelegate(
      GetV8MemoryUsageCallback&& callback)
      : callback_(std::move(callback)) {}

  ~FrameAssociatedMeasurementDelegate() override {
    if (callback_) {
      std::move(callback_).Run(mojom::blink::PerProcessV8MemoryUsage::New());
    }
  }

 private:
  bool ShouldMeasure(v8::Local<v8::Context> context) override {
    // Measure all contexts.
    return true;
  }

  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override {
    mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage =
        mojom::blink::PerIsolateV8MemoryUsage::New();
    for (const auto& context_and_size : context_sizes_in_bytes) {
      const v8::Local<v8::Context>& context = context_and_size.first;
      const size_t size = context_and_size.second;

      LocalFrame* frame = ToLocalFrameIfNotDetached(context);

      if (!frame) {
        // TODO(crbug.com/1080672): It would be prefereable to count the
        // V8SchemaRegistry context's overhead with unassociated_bytes, but at
        // present there isn't a public API that allows this distinction.
        ++(isolate_memory_usage->num_unassociated_contexts);
        isolate_memory_usage->unassociated_context_bytes_used += size;
        continue;
      }
      if (DOMWrapperWorld::World(context).GetWorldId() !=
          DOMWrapperWorld::kMainWorldId) {
        // TODO(crbug.com/1085129): Handle extension contexts once they get
        // their own V8ContextToken.
        continue;
      }
      auto context_memory_usage = mojom::blink::PerContextV8MemoryUsage::New();
      context_memory_usage->token =
          frame->DomWindow()->GetExecutionContextToken();
      context_memory_usage->bytes_used = size;
#if DCHECK_IS_ON()
      // Check that the token didn't already occur.
      for (const auto& entry : isolate_memory_usage->contexts) {
        DCHECK_NE(entry->token, context_memory_usage->token);
      }
#endif
      isolate_memory_usage->contexts.push_back(std::move(context_memory_usage));
    }

    mojom::blink::PerProcessV8MemoryUsagePtr result =
        mojom::blink::PerProcessV8MemoryUsage::New();
    result->isolates.push_back(std::move(isolate_memory_usage));
    std::move(callback_).Run(std::move(result));
  }

  GetV8MemoryUsageCallback callback_;
};

v8::MeasureMemoryExecution ToV8MeasureMemoryExecution(
    V8DetailedMemoryReporterImpl::Mode mode) {
  switch (mode) {
    case V8DetailedMemoryReporterImpl::Mode::DEFAULT:
      return v8::MeasureMemoryExecution::kDefault;
    case V8DetailedMemoryReporterImpl::Mode::EAGER:
      return v8::MeasureMemoryExecution::kEager;
    case V8DetailedMemoryReporterImpl::Mode::LAZY:
      return v8::MeasureMemoryExecution::kLazy;
  }
  NOTREACHED();
}

}  // namespace

// static
void V8DetailedMemoryReporterImpl::Create(
    mojo::PendingReceiver<mojom::blink::V8DetailedMemoryReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8DetailedMemoryReporterImpl>(),
                              std::move(receiver));
}

void V8DetailedMemoryReporterImpl::GetV8MemoryUsage(
    V8DetailedMemoryReporterImpl::Mode mode,
    GetV8MemoryUsageCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate) {
    std::move(callback).Run(mojom::blink::PerProcessV8MemoryUsage::New());
  } else {
    std::unique_ptr<FrameAssociatedMeasurementDelegate> delegate =
        std::make_unique<FrameAssociatedMeasurementDelegate>(
            std::move(callback));

    isolate->MeasureMemory(std::move(delegate),
                           ToV8MeasureMemoryExecution(mode));
  }
}

}  // namespace blink
