// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/javascript_call_stack_generator.h"

#include "third_party/blink/renderer/controller/javascript_call_stack_collector.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

JavaScriptCallStackGenerator& GetJavaScriptCallStackGenerator() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(JavaScriptCallStackGenerator,
                                  javascript_call_stack_generator, ());
  return javascript_call_stack_generator;
}

}  // namespace

void JavaScriptCallStackGenerator::OnCollectorFinished(
    JavaScriptCallStackCollector* collector) {
  collectors_.erase(collector);
}

void JavaScriptCallStackGenerator::CollectJavaScriptCallStack(
    CollectJavaScriptCallStackCallback callback) {
    std::unique_ptr<JavaScriptCallStackCollector> call_stack_collector =
        std::make_unique<JavaScriptCallStackCollector>(
            std::move(callback),
            WTF::BindOnce(&JavaScriptCallStackGenerator::OnCollectorFinished,
                          WTF::Unretained(this)));
    JavaScriptCallStackCollector* raw_collector = call_stack_collector.get();
    collectors_.Set(raw_collector, std::move(call_stack_collector));
    raw_collector->CollectJavaScriptCallStack();
}

void JavaScriptCallStackGenerator::Bind(
    mojo::PendingReceiver<mojom::blink::CallStackGenerator> receiver) {
  DCHECK(!GetJavaScriptCallStackGenerator().receiver_.is_bound());
  GetJavaScriptCallStackGenerator().receiver_.Bind(std::move(receiver));
}

}  // namespace blink
