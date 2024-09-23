/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "gin/public/v8_idle_task_runner.h"
#include "partition_alloc/oom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_histogram_accumulator.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/bindings/v8_value_cache.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"

namespace blink {

BASE_FEATURE(kTaskAttributionInfrastructureDisabledForTesting,
             "TaskAttributionInfrastructureDisabledForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

void AddCrashKey(v8::CrashKeyId id, const std::string& value) {
  using base::debug::AllocateCrashKeyString;
  using base::debug::CrashKeySize;
  using base::debug::SetCrashKeyString;

  switch (id) {
    case v8::CrashKeyId::kIsolateAddress:
      static auto* const isolate_address =
          AllocateCrashKeyString("v8_isolate_address", CrashKeySize::Size32);
      SetCrashKeyString(isolate_address, value);
      break;
    case v8::CrashKeyId::kReadonlySpaceFirstPageAddress:
      static auto* const ro_space_firstpage_address = AllocateCrashKeyString(
          "v8_ro_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(ro_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kMapSpaceFirstPageAddress:
      static auto* const map_space_firstpage_address = AllocateCrashKeyString(
          "v8_map_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(map_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kCodeSpaceFirstPageAddress:
      static auto* const code_space_firstpage_address = AllocateCrashKeyString(
          "v8_code_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(code_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kDumpType:
      static auto* const dump_type =
          AllocateCrashKeyString("dump-type", CrashKeySize::Size32);
      SetCrashKeyString(dump_type, value);
      break;
    default:
      // Doing nothing for new keys is a valid option. Having this case allows
      // to introduce new CrashKeyId's without triggering a build break.
      break;
  }
}

V8PerIsolateData::TaskAttributionTrackerFactoryPtr
    task_attribution_tracker_factory = nullptr;

}  // namespace

static void BeforeCallEnteredCallback(v8::Isolate* isolate) {
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
}

static bool AllowAtomicWaits(
    V8PerIsolateData::V8ContextSnapshotMode v8_context_snapshot_mode) {
  return !IsMainThread() ||
         v8_context_snapshot_mode ==
             V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot;
}

V8PerIsolateData::V8PerIsolateData(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner,
    V8ContextSnapshotMode v8_context_snapshot_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback)
    : v8_context_snapshot_mode_(v8_context_snapshot_mode),
      isolate_holder_(
          std::move(task_runner),
          gin::IsolateHolder::kSingleThread,
          AllowAtomicWaits(v8_context_snapshot_mode)
              ? gin::IsolateHolder::kAllowAtomicsWait
              : gin::IsolateHolder::kDisallowAtomicsWait,
          IsMainThread() ? gin::IsolateHolder::IsolateType::kBlinkMainThread
                         : gin::IsolateHolder::IsolateType::kBlinkWorkerThread,
          v8_context_snapshot_mode ==
                  V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot
              ? gin::IsolateHolder::IsolateCreationMode::kCreateSnapshot
              : gin::IsolateHolder::IsolateCreationMode::kNormal,
          create_histogram_callback,
          add_histogram_sample_callback,
          std::move(user_visible_task_runner),
          std::move(best_effort_task_runner)),
      string_cache_(std::make_unique<StringCache>(GetIsolate())),
      private_property_(std::make_unique<V8PrivateProperty>()),
      constructor_mode_(ConstructorMode::kCreateNewObject),
      runtime_call_stats_(base::DefaultTickClock::GetInstance()) {
  if (v8_context_snapshot_mode == V8ContextSnapshotMode::kTakeSnapshot) {
    // Snapshot should only execute on the main thread. SnapshotCreator enters
    // the isolate, so we don't call Isolate::Enter() here.
    CHECK(IsMainThread());
  } else {
    // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
    GetIsolate()->Enter();
    GetIsolate()->AddBeforeCallEnteredCallback(&BeforeCallEnteredCallback);
  }
  if (IsMainThread()) {
    GetIsolate()->SetAddCrashKeyCallback(AddCrashKey);
    main_world_ =
        DOMWrapperWorld::Create(GetIsolate(), DOMWrapperWorld::WorldType::kMain,
                                /*is_default_world_of_isolate=*/true);
    if (!base::FeatureList::IsEnabled(
            kTaskAttributionInfrastructureDisabledForTesting)) {
      CHECK(task_attribution_tracker_factory);
      task_attribution_tracker_ =
          task_attribution_tracker_factory(GetIsolate());
    }
  }
}

V8PerIsolateData::~V8PerIsolateData() = default;

v8::Isolate* V8PerIsolateData::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner,
    V8ContextSnapshotMode context_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback) {
  TRACE_EVENT1("v8", "V8PerIsolateData::Initialize", "V8ContextSnapshotMode",
               context_mode);
  V8PerIsolateData* data = new V8PerIsolateData(
      std::move(task_runner), std::move(user_visible_task_runner),
      std::move(best_effort_task_runner), context_mode,
      create_histogram_callback, add_histogram_sample_callback);
  DCHECK(data);

  v8::Isolate* isolate = data->GetIsolate();
  isolate->SetData(gin::kEmbedderBlink, data);
  return isolate;
}

void V8PerIsolateData::EnableIdleTasks(
    v8::Isolate* isolate,
    std::unique_ptr<gin::V8IdleTaskRunner> task_runner) {
  From(isolate)->isolate_holder_.EnableIdleTasks(std::move(task_runner));
}

// willBeDestroyed() clear things that should be cleared before
// ThreadState::detach() gets called.
void V8PerIsolateData::WillBeDestroyed(v8::Isolate* isolate) {
  V8PerIsolateData* data = From(isolate);

  data->thread_debugger_.reset();

  for (auto& item : data->user_data_) {
    if (item) {
      item->WillBeDestroyed();
    }
  }

  data->ClearScriptRegexpContext();

  ThreadState::Current()->DetachFromIsolate();

  data->active_script_wrappable_manager_.Clear();
  // Callbacks can be removed as they only cover single events (e.g. atomic
  // pause) and they cannot get out of sync.
  DCHECK_EQ(0u, data->gc_callback_depth_);
  isolate->RemoveGCPrologueCallback(data->prologue_callback_);
  isolate->RemoveGCEpilogueCallback(data->epilogue_callback_);
}

void V8PerIsolateData::SetGCCallbacks(
    v8::Isolate* isolate,
    v8::Isolate::GCCallback prologue_callback,
    v8::Isolate::GCCallback epilogue_callback) {
  prologue_callback_ = prologue_callback;
  epilogue_callback_ = epilogue_callback;
  isolate->AddGCPrologueCallback(prologue_callback_);
  isolate->AddGCEpilogueCallback(epilogue_callback_);
}

// destroy() clear things that should be cleared after ThreadState::detach()
// gets called but before the Isolate exits.
void V8PerIsolateData::Destroy(v8::Isolate* isolate) {
  isolate->RemoveBeforeCallEnteredCallback(&BeforeCallEnteredCallback);
  V8PerIsolateData* data = From(isolate);

  // Clear everything before exiting the Isolate.
  if (data->script_regexp_script_state_) {
    data->script_regexp_script_state_->DisposePerContextData();
  }
  data->private_property_.reset();
  data->string_cache_->Dispose();
  data->string_cache_.reset();
  data->v8_template_map_for_main_world_.clear();
  data->v8_template_map_for_non_main_worlds_.clear();

  // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
  isolate->Exit();
  delete data;
}

v8::Local<v8::Template> V8PerIsolateData::FindV8Template(
    const DOMWrapperWorld& world,
    const void* key) {
  auto& map = SelectV8TemplateMap(world);
  auto result = map.find(key);
  if (result != map.end())
    return result->value.Get(GetIsolate());
  return v8::Local<v8::Template>();
}

void V8PerIsolateData::AddV8Template(const DOMWrapperWorld& world,
                                     const void* key,
                                     v8::Local<v8::Template> value) {
  auto& map = SelectV8TemplateMap(world);
  auto result = map.insert(key, v8::Eternal<v8::Template>(GetIsolate(), value));
  DCHECK(result.is_new_entry);
}

v8::MaybeLocal<v8::DictionaryTemplate>
V8PerIsolateData::FindV8DictionaryTemplate(const void* key) {
  auto it = v8_dict_template_map_.find(key);
  return it != v8_dict_template_map_.end()
             ? it->value.Get(GetIsolate())
             : v8::MaybeLocal<v8::DictionaryTemplate>();
}

void V8PerIsolateData::AddV8DictionaryTemplate(
    const void* key,
    v8::Local<v8::DictionaryTemplate> value) {
  auto result = v8_dict_template_map_.insert(
      key, v8::Eternal<v8::DictionaryTemplate>(GetIsolate(), value));
  DCHECK(result.is_new_entry);
}

bool V8PerIsolateData::HasInstance(const WrapperTypeInfo* wrapper_type_info,
                                   v8::Local<v8::Value> untrusted_value) {
  RUNTIME_CALL_TIMER_SCOPE(GetIsolate(),
                           RuntimeCallStats::CounterId::kHasInstance);
  return HasInstance(wrapper_type_info, untrusted_value,
                     v8_template_map_for_main_world_) ||
         HasInstance(wrapper_type_info, untrusted_value,
                     v8_template_map_for_non_main_worlds_);
}

bool V8PerIsolateData::HasInstance(const WrapperTypeInfo* wrapper_type_info,
                                   v8::Local<v8::Value> untrusted_value,
                                   const V8TemplateMap& map) {
  auto result = map.find(wrapper_type_info);
  if (result == map.end())
    return false;
  v8::Local<v8::Template> v8_template = result->value.Get(GetIsolate());
  DCHECK(v8_template->IsFunctionTemplate());
  return v8_template.As<v8::FunctionTemplate>()->HasInstance(untrusted_value);
}

bool V8PerIsolateData::HasInstanceOfUntrustedType(
    const WrapperTypeInfo* untrusted_wrapper_type_info,
    v8::Local<v8::Value> untrusted_value) {
  RUNTIME_CALL_TIMER_SCOPE(GetIsolate(),
                           RuntimeCallStats::CounterId::kHasInstance);
  return HasInstanceOfUntrustedType(untrusted_wrapper_type_info,
                                    untrusted_value,
                                    v8_template_map_for_main_world_) ||
         HasInstanceOfUntrustedType(untrusted_wrapper_type_info,
                                    untrusted_value,
                                    v8_template_map_for_non_main_worlds_);
}

bool V8PerIsolateData::HasInstanceOfUntrustedType(
    const WrapperTypeInfo* untrusted_wrapper_type_info,
    v8::Local<v8::Value> untrusted_value,
    const V8TemplateMap& map) {
  auto result = map.find(untrusted_wrapper_type_info);
  if (result == map.end())
    return false;
  v8::Local<v8::Template> v8_template = result->value.Get(GetIsolate());
  if (!v8_template->IsFunctionTemplate())
    return false;
  return v8_template.As<v8::FunctionTemplate>()->HasInstance(untrusted_value);
}

V8PerIsolateData::V8TemplateMap& V8PerIsolateData::SelectV8TemplateMap(
    const DOMWrapperWorld& world) {
  return world.IsMainWorld() ? v8_template_map_for_main_world_
                             : v8_template_map_for_non_main_worlds_;
}

void V8PerIsolateData::ClearPersistentsForV8ContextSnapshot() {
  v8_template_map_for_main_world_.clear();
  v8_template_map_for_non_main_worlds_.clear();
  eternal_name_cache_.clear();
  private_property_.reset();
}

const base::span<const v8::Eternal<v8::Name>>
V8PerIsolateData::FindOrCreateEternalNameCache(
    const void* lookup_key,
    base::span<const std::string_view> names) {
  auto it = eternal_name_cache_.find(lookup_key);
  const Vector<v8::Eternal<v8::Name>>* vector = nullptr;
  if (it == eternal_name_cache_.end()) [[unlikely]] {
    v8::Isolate* isolate = GetIsolate();
    Vector<v8::Eternal<v8::Name>> new_vector(
        base::checked_cast<wtf_size_t>(names.size()));
    base::ranges::transform(
        names, new_vector.begin(), [isolate](std::string_view name) {
          return v8::Eternal<v8::Name>(
              isolate,
              V8AtomicString(
                  isolate,
                  StringView(name.data(), static_cast<unsigned>(name.size()))));
        });
    vector = &eternal_name_cache_.Set(lookup_key, std::move(new_vector))
                  .stored_value->value;
  } else {
    vector = &it->value;
  }
  DCHECK_EQ(vector->size(), names.size());
  return base::span<const v8::Eternal<v8::Name>>(vector->data(),
                                                 vector->size());
}

v8::Local<v8::Context> V8PerIsolateData::EnsureScriptRegexpContext() {
  if (!script_regexp_script_state_) {
    LEAK_SANITIZER_DISABLED_SCOPE;
    v8::Local<v8::Context> context(v8::Context::New(GetIsolate()));
    script_regexp_script_state_ = ScriptState::Create(
        context,
        DOMWrapperWorld::Create(GetIsolate(),
                                DOMWrapperWorld::WorldType::kRegExp),
        /* execution_context = */ nullptr);
  }
  return script_regexp_script_state_->GetContext();
}

void V8PerIsolateData::ClearScriptRegexpContext() {
  if (script_regexp_script_state_) {
    script_regexp_script_state_->DisposePerContextData();
    script_regexp_script_state_->DissociateContext();
  }
  script_regexp_script_state_ = nullptr;
}

void V8PerIsolateData::SetThreadDebugger(
    std::unique_ptr<ThreadDebugger> thread_debugger) {
  DCHECK(!thread_debugger_);
  thread_debugger_ = std::move(thread_debugger);
}

void V8PerIsolateData::SetPasswordRegexp(ScriptRegexp* password_regexp) {
  password_regexp_ = password_regexp;
}

ScriptRegexp* V8PerIsolateData::GetPasswordRegexp() {
  return password_regexp_;
}

void V8PerIsolateData::SetTaskAttributionTrackerFactory(
    TaskAttributionTrackerFactoryPtr factory) {
  CHECK(!task_attribution_tracker_factory);
  CHECK(IsMainThread());
  task_attribution_tracker_factory = factory;
}

void* CreateHistogram(const char* name, int min, int max, size_t buckets) {
  // Each histogram has an implicit '0' bucket (for underflow), so we can always
  // bump the minimum to 1.
  DCHECK_LE(0, min);
  min = std::max(1, min);

  // For boolean histograms, always include an overflow bucket [2, infinity).
  if (max == 1 && buckets == 2) {
    max = 2;
    buckets = 3;
  }

  const std::string histogram_name =
      Platform::Current()->GetNameForHistogram(name);
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      histogram_name, min, max, static_cast<uint32_t>(buckets),
      base::Histogram::kUmaTargetedHistogramFlag);

  return V8HistogramAccumulator::GetInstance()->RegisterHistogram(
      histogram, histogram_name);
}

void AddHistogramSample(void* hist, int sample) {
  V8HistogramAccumulator::GetInstance()->AddSample(hist, sample);
}

}  // namespace blink
