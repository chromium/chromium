/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite_helper.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "gin/public/v8_platform.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_platform.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mock_mime_registry.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class TestingPlatformSupport::TestingBrowserInterfaceBroker
    : public ThreadSafeBrowserInterfaceBrokerProxy {
 public:
  TestingBrowserInterfaceBroker() = default;
  ~TestingBrowserInterfaceBroker() override = default;

  void GetInterfaceImpl(mojo::GenericPendingReceiver receiver) override {
    auto& override_callback = GetOverrideCallback();
    auto interface_name = receiver.interface_name().value_or("");
    if (!override_callback.is_null()) {
      override_callback.Run(interface_name.c_str(), receiver.PassPipe());
      return;
    }
    if (interface_name == mojom::blink::MimeRegistry::Name_) {
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<MockMimeRegistry>(),
          mojo::PendingReceiver<mojom::blink::MimeRegistry>(
              receiver.PassPipe()));
      return;
    }
  }

  static ScopedOverrideMojoInterface::GetInterfaceCallback&
  GetOverrideCallback() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        ScopedOverrideMojoInterface::GetInterfaceCallback, callback, ());
    return callback;
  }
};

TestingPlatformSupport::ScopedOverrideMojoInterface::
    ScopedOverrideMojoInterface(GetInterfaceCallback callback)
    : auto_reset_(&TestingBrowserInterfaceBroker::GetOverrideCallback(),
                  std::move(callback)) {}

TestingPlatformSupport::ScopedOverrideMojoInterface::
    ~ScopedOverrideMojoInterface() = default;

TestingPlatformSupport::TestingPlatformSupport()
    : old_platform_(Platform::Current()),
      interface_broker_(base::MakeRefCounted<TestingBrowserInterfaceBroker>()) {
  DCHECK(old_platform_);
  DCHECK(IsMainThread());
}

TestingPlatformSupport::~TestingPlatformSupport() {
  DCHECK_EQ(this, Platform::Current());
}

WebString TestingPlatformSupport::DefaultLocale() {
  return WebString::FromUTF8("en-US");
}

WebData TestingPlatformSupport::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return old_platform_
             ? old_platform_->GetDataResource(resource_id, scale_factor)
             : WebData();
}

std::string TestingPlatformSupport::GetDataResourceString(int resource_id) {
  return old_platform_ ? old_platform_->GetDataResourceString(resource_id)
                       : std::string();
}

ThreadSafeBrowserInterfaceBrokerProxy*
TestingPlatformSupport::GetBrowserInterfaceBroker() {
  return interface_broker_.get();
}

// ValueConverter only for simple data types used in tests.
class V8ValueConverterForTest final : public WebV8ValueConverter {
 public:
  void SetDateAllowed(bool val) override {}
  void SetRegExpAllowed(bool val) override {}

  v8::Local<v8::Value> ToV8Value(base::ValueView value,
                                 v8::Local<v8::Context> context) override {
    // The following logic is forked from V8ValueConverterImpl::ToV8Value() to
    // support converting basic types.
    struct Visitor {
      raw_ptr<const V8ValueConverterForTest> converter;
      raw_ptr<v8::Isolate> isolate;
      v8::Local<v8::Object> creation_context;

      v8::Local<v8::Value> operator()(std::monostate value) {
        return v8::Null(isolate);
      }

      v8::Local<v8::Value> operator()(bool value) {
        return v8::Boolean::New(isolate, value);
      }

      v8::Local<v8::Value> operator()(int value) {
        return v8::Integer::New(isolate, value);
      }

      v8::Local<v8::Value> operator()(double value) {
        return v8::Number::New(isolate, value);
      }

      v8::Local<v8::Value> operator()(std::string_view value) {
        return v8::String::NewFromUtf8(isolate, value.data(),
                                       v8::NewStringType::kNormal,
                                       value.length())
            .ToLocalChecked();
      }

      v8::Local<v8::Value> operator()(const base::Value::BlobStorage& value) {
        NOTREACHED();
      }

      v8::Local<v8::Value> operator()(const base::Value::Dict& value) {
        NOTREACHED();
      }

      v8::Local<v8::Value> operator()(const base::Value::List& value) {
        NOTREACHED();
      }
    };

    return value.Visit(Visitor{.converter = this,
                               .isolate = v8::Isolate::GetCurrent(),
                               .creation_context = context->Global()});
  }

  std::unique_ptr<base::Value> FromV8Value(
      v8::Local<v8::Value> val,
      v8::Local<v8::Context> context) override {
    CHECK(!val.IsEmpty());

    v8::Context::Scope context_scope(context);
    auto* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handle_scope(isolate);

    if (val->IsBoolean()) {
      return std::make_unique<base::Value>(
          base::Value(val->ToBoolean(isolate)->Value()));
    }

    if (val->IsInt32()) {
      return std::make_unique<base::Value>(
          base::Value(val.As<v8::Int32>()->Value()));
    }

    if (val->IsString()) {
      v8::String::Utf8Value utf8(isolate, val);
      return std::make_unique<base::Value>(
          base::Value(std::string(*utf8, utf8.length())));
    }

    // The following logic is forked from V8ValueConverterImpl::FromV8Object()
    // to support converting ScriptObject in tests.
    if (val->IsObject()) {
      v8::Local<v8::Object> val_obj = val.As<v8::Object>();
      base::Value::Dict result;
      v8::Local<v8::Array> property_names;
      if (!val_obj->GetOwnPropertyNames(isolate->GetCurrentContext())
               .ToLocal(&property_names)) {
        return std::make_unique<base::Value>(std::move(result));
      }

      for (uint32_t i = 0; i < property_names->Length(); ++i) {
        v8::Local<v8::Value> key =
            property_names->Get(isolate->GetCurrentContext(), i)
                .ToLocalChecked();

        // Extend this test to cover more types as necessary and if sensible.
        if (!key->IsString() && !key->IsNumber()) {
          NOTREACHED() << "Key \"" << *v8::String::Utf8Value(isolate, key)
                       << "\" "
                          "is neither a string nor a number";
        }

        v8::String::Utf8Value name_utf8(isolate, key);

        v8::TryCatch try_catch(isolate);
        v8::Local<v8::Value> child_v8;
        v8::MaybeLocal<v8::Value> maybe_child =
            val_obj->Get(isolate->GetCurrentContext(), key);
        if (try_catch.HasCaught() || !maybe_child.ToLocal(&child_v8)) {
          LOG(WARNING) << "Getter for property " << *name_utf8
                       << " threw an exception.";
          child_v8 = v8::Null(isolate);
        }

        std::unique_ptr<base::Value> child = FromV8Value(child_v8, context);
        if (!child) {
          // JSON.stringify skips properties whose values don't serialize, for
          // example undefined and functions. Emulate that behavior.
          continue;
        }

        result.Set(std::string(*name_utf8, name_utf8.length()),
                   std::move(*child));
      }
      return std::make_unique<base::Value>(std::move(result));
    }

    // Returns `nullptr` for a broader range of values than actual
    // `V8ValueConverter`.
    return nullptr;
  }
};

std::unique_ptr<blink::WebV8ValueConverter>
TestingPlatformSupport::CreateWebV8ValueConverter() {
  return std::make_unique<V8ValueConverterForTest>();
}

void TestingPlatformSupport::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

bool TestingPlatformSupport::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

void TestingPlatformSupport::SetThreadedAnimationEnabled(bool enabled) {
  is_threaded_animation_enabled_ = enabled;
}

const base::Clock* TestingPlatformSupport::GetClock() const {
  return base::DefaultClock::GetInstance();
}

const base::TickClock* TestingPlatformSupport::GetTickClock() const {
  return base::DefaultTickClock::GetInstance();
}

base::TimeTicks TestingPlatformSupport::NowTicks() const {
  return base::TimeTicks::Now();
}

ScopedUnittestsEnvironmentSetup::ScopedUnittestsEnvironmentSetup(int argc,
                                                                 char** argv) {
  base::CommandLine::Init(argc, argv);

  base::test::InitializeICUForTesting();

  discardable_memory_allocator_ =
      std::make_unique<base::TestDiscardableMemoryAllocator>();
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_memory_allocator_.get());

  // FeatureList must be initialized before Partitions::Initialize(),
  // because Partitions::Initialize() uses base::FeatureList to obtain
  // PartitionOptions.
  // NOTE: InitScopedFeatureListForTesting() deliberately removes
  // `--enable-features` and `--disable-features` from the command line of the
  // current process after processing them. However, here that behavior is
  // undesirable: This helper object is used by Blink test suites to configure
  // their environment before they run their tests via the base::TestSuite
  // infrastructure, which itself queries `--enable-features` and
  // `--disable-features` from the command line in order to configure the
  // environment that the tests run in.
  base::CommandLine command_line = *base::CommandLine::ForCurrentProcess();
  base::test::InitScopedFeatureListForTesting(scoped_feature_list_);
  *base::CommandLine::ForCurrentProcess() = command_line;

  // TODO(yutak): The initialization steps below are essentially a subset of
  // Platform::Initialize() steps with a few modifications for tests.
  // We really shouldn't have those initialization steps in two places,
  // because they are a very fragile piece of code (the initialization order
  // is so sensitive) and we want it to be consistent between tests and
  // production. Fix this someday.
  dummy_platform_ = std::make_unique<Platform>();
  Platform::SetCurrentPlatformForTesting(dummy_platform_.get());

  Partitions::Initialize();
  InitializeWtf();
  Length::Initialize();

  // This must be called after blink::InitializeWtf(), because ThreadSpecific<>
  // used in this function depends on blink::IsMainThread().
  Platform::CreateMainThreadForTesting();

  testing_platform_support_ = std::make_unique<TestingPlatformSupport>();
  Platform::SetCurrentPlatformForTesting(testing_platform_support_.get());

  ProcessHeap::Init();
  // Initializing ThreadState for testing with a testing specific platform.
  // ScopedUnittestsEnvironmentSetup keeps the platform alive until the end of
  // the test. The testing platform is initialized using gin::V8Platform which
  // is the default platform used by ThreadState.
  // Note that the platform is not initialized by AttachMainThreadForTesting
  // to avoid including test-only headers in production build targets.
  v8_platform_for_heap_testing_ =
      std::make_unique<HeapTestingPlatformAdapter>(gin::V8Platform::Get());
  ThreadState::AttachMainThreadForTesting(v8_platform_for_heap_testing_.get());
  conservative_gc_scope_.emplace(ThreadState::Current());
  http_names::Init();
  fetch_initiator_type_names::Init();

  InitializePlatformLanguage();
  font_family_names::Init();
  WebRuntimeFeatures::EnableExperimentalFeatures(true);
  WebRuntimeFeatures::EnableTestOnlyFeatures(true);
}

ScopedUnittestsEnvironmentSetup::~ScopedUnittestsEnvironmentSetup() = default;

}  // namespace blink
