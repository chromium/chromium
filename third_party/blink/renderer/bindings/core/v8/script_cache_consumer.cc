// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_cache_consumer.h"

#include <atomic>
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/blink/renderer/bindings/core/v8/script_cache_consumer_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "v8/include/v8.h"

namespace blink {

ScriptCacheConsumer::ScriptCacheConsumer(
    v8::Isolate* isolate,
    scoped_refptr<CachedMetadata> cached_metadata,
    const String& script_url_string,
    uint64_t script_resource_identifier)
    : isolate_(isolate),
      cached_metadata_(cached_metadata),
      script_url_string_(script_url_string),
      script_resource_identifier_(script_resource_identifier),
      state_(State::kRunning) {
  consume_task_.reset(v8::ScriptCompiler::StartConsumingCodeCache(
      isolate_, V8CodeCache::CreateCachedData(cached_metadata_)));

  if (consume_task_) {
    TRACE_EVENT_WITH_FLOW1(
        "v8," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
        "v8.deserializeOnBackground.start", TRACE_ID_LOCAL(this),
        TRACE_EVENT_FLAG_FLOW_OUT, "data", [&](perfetto::TracedValue context) {
          inspector_deserialize_script_event::Data(std::move(context),
                                                   script_resource_identifier_,
                                                   script_url_string_);
        });

    worker_pool::PostTask(FROM_HERE, WTF::CrossThreadBindOnce(
                                         &ScriptCacheConsumer::RunTaskOffThread,
                                         WrapCrossThreadWeakPersistent(this)));
  } else {
    // If the consume task failed to be created, consider the consumption
    // immediately completed. TakeV8ConsumeTask will return nullptr, but this is
    // allowed.
    AdvanceState(State::kConsumeFinished);
  }
}

ScriptCacheConsumer::ScriptCacheConsumer(
    v8::Isolate* isolate,
    scoped_refptr<CachedMetadata> cached_metadata,
    std::unique_ptr<v8::ScriptCompiler::ConsumeCodeCacheTask>
        completed_consume_task,
    const String& script_url_string,
    uint64_t script_resource_identifier)
    : isolate_(isolate),
      cached_metadata_(std::move(cached_metadata)),
      consume_task_(std::move(completed_consume_task)),
      script_url_string_(script_url_string),
      script_resource_identifier_(script_resource_identifier),
      state_(State::kRunning) {
  CHECK(consume_task_);
  AdvanceState(State::kConsumeFinished);
}

ScriptCacheConsumer::State ScriptCacheConsumer::AdvanceState(
    State new_state_bit) {
  // We should only be setting a single state bit at a time.
  DCHECK(new_state_bit == State::kConsumeFinished ||
         new_state_bit == State::kClientReady ||
         new_state_bit == State::kMergeDoneOrNotNeededBit ||
         new_state_bit == State::kCalledFinishCallbackBit);

  State state = state_.load(std::memory_order_relaxed);
  while (true) {
    // Since we're setting the new state bit now, it shouldn't have been set on
    // the state before now.
    DCHECK_EQ(state & new_state_bit, 0);

    // Set the new state bit on the state, and update the state atomically.
    State new_state = static_cast<State>(state | new_state_bit);
    if (state_.compare_exchange_strong(state, new_state)) {
      return new_state;
    }
  }
}

void ScriptCacheConsumer::RunTaskOffThread() {
  DCHECK(!WTF::IsMainThread());

  TRACE_EVENT_WITH_FLOW1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.deserializeOnBackground", TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "data",
      [&](perfetto::TracedValue context) {
        inspector_deserialize_script_event::Data(std::move(context),
                                                 script_resource_identifier_,
                                                 script_url_string_);
      });

  // Run the cache consumption task.
  consume_task_->Run();

  State new_state = AdvanceState(State::kConsumeFinished);
  if (new_state == State::kFinishedAndReady) {
    finish_trace_name_ = "v8.deserializeOnBackground.finishedAfterResource";
    if (consume_task_->ShouldMergeWithExistingScript()) {
      RunMergeTaskOffThread();
    } else {
      AdvanceState(State::kMergeDoneOrNotNeededBit);
      PostFinishCallbackTask();
    }
  }
}

void ScriptCacheConsumer::PostFinishCallbackTask() {
  DCHECK(!WTF::IsMainThread());
  CHECK(finish_callback_task_runner_);
  PostCrossThreadTask(
      *finish_callback_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&ScriptCacheConsumer::CallFinishCallback,
                               WrapCrossThreadWeakPersistent(this)));
}

void ScriptCacheConsumer::RunMergeTaskOffThread() {
  DCHECK(!WTF::IsMainThread());
  DCHECK_EQ(state_, State::kFinishedAndReady);

  TRACE_EVENT_WITH_FLOW1(
      "v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "v8.deserializeOnBackground.mergeWithExistingScript",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "data",
      [&](perfetto::TracedValue context) {
        inspector_deserialize_script_event::Data(std::move(context),
                                                 script_resource_identifier_,
                                                 script_url_string_);
      });

  consume_task_->MergeWithExistingScript();

  AdvanceState(State::kMergeDoneOrNotNeededBit);
  PostFinishCallbackTask();
}

void ScriptCacheConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(finish_callback_client_);
}

void ScriptCacheConsumer::NotifyClientWaiting(
    ScriptCacheConsumerClient* client,
    ClassicScript* classic_script,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(WTF::IsMainThread());

  CHECK(!finish_callback_client_);
  finish_callback_client_ = client;

  // Set the task runner before advancing the state, to prevent a race between
  // this advancing to kResourceFinished and the off-thread task advancing to
  // kBothFinished and wanting to post using the task runner.
  CHECK(!finish_callback_task_runner_);
  finish_callback_task_runner_ = task_runner;

  {
    v8::HandleScope scope(isolate_);
    const ParkableString& source_text = classic_script->SourceText();
    v8::ScriptOrigin origin = classic_script->CreateScriptOrigin(isolate_);
    if (consume_task_) {
      consume_task_->SourceTextAvailable(
          isolate_, V8String(isolate_, source_text), origin);
    }
  }

  State new_state = AdvanceState(State::kClientReady);
  if (new_state == State::kFinishedAndReady) {
    finish_trace_name_ = "v8.deserializeOnBackground.finishedBeforeResource";
    if (consume_task_ && consume_task_->ShouldMergeWithExistingScript()) {
      TRACE_EVENT_WITH_FLOW1(
          "v8," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
          "v8.deserializeOnBackground.startMergeWithExistingScript",
          TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT, "data",
          [&](perfetto::TracedValue context) {
            inspector_deserialize_script_event::Data(
                std::move(context), script_resource_identifier_,
                script_url_string_);
          });

      worker_pool::PostTask(
          FROM_HERE,
          WTF::CrossThreadBindOnce(&ScriptCacheConsumer::RunMergeTaskOffThread,
                                   WrapCrossThreadWeakPersistent(this)));
    } else {
      AdvanceState(State::kMergeDoneOrNotNeededBit);
      CallFinishCallback();
    }
  }
}

void ScriptCacheConsumer::CallFinishCallback() {
  DCHECK(WTF::IsMainThread());

  ScriptCacheConsumerClient* client = finish_callback_client_.Get();

  // The resource is a weak member, so it may have been collected.
  if (!client)
    return;

  TRACE_EVENT_WITH_FLOW1("v8," TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         finish_trace_name_, TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN, "data",
                         [&](perfetto::TracedValue context) {
                           inspector_deserialize_script_event::Data(
                               std::move(context), script_resource_identifier_,
                               script_url_string_);
                         });

  CHECK_EQ(state_, State::kMergeDoneOrNotNeeded);
  // Clear the task runner, we don't need it anymore since we've already made
  // our way to the main thread.
  finish_callback_task_runner_.reset();
  AdvanceState(State::kCalledFinishCallbackBit);
  client->NotifyCacheConsumeFinished();
}

}  // namespace blink
