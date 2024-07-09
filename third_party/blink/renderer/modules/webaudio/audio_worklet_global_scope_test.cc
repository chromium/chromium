// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

namespace {

constexpr size_t kRenderQuantumFrames = 128;

}  // namespace

// The test uses OfflineAudioWorkletThread because the test does not have a
// strict real-time constraint.
class AudioWorkletGlobalScopeTest : public PageTestBase, public ModuleTestBase {
 public:
  void SetUp() override {
    ModuleTestBase::SetUp();
    PageTestBase::SetUp(gfx::Size());
    NavigateTo(KURL("https://example.com/"));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    ModuleTestBase::TearDown();
  }

  std::unique_ptr<OfflineAudioWorkletThread> CreateAudioWorkletThread() {
    std::unique_ptr<OfflineAudioWorkletThread> thread =
        std::make_unique<OfflineAudioWorkletThread>(*reporting_proxy_);
    LocalDOMWindow* window = GetFrame().DomWindow();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            window->Url(), mojom::blink::ScriptType::kModule, "AudioWorklet",
            window->UserAgent(),
            window->GetFrame()->Loader().UserAgentMetadata(),
            nullptr /* web_worker_fetch_context */,
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            window->GetReferrerPolicy(), window->GetSecurityOrigin(),
            window->IsSecureContext(), window->GetHttpsState(),
            nullptr /* worker_clients */, nullptr /* content_settings_client */,
            OriginTrialContext::GetInheritedTrialFeatures(window).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            mojom::blink::V8CacheOptions::kDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>(),
            mojo::NullRemote() /* browser_interface_broker */,
            window->GetFrame()->Loader().CreateWorkerCodeCacheHost(),
            window->GetFrame()->GetBlobUrlStorePendingRemote(),
            BeginFrameProviderParams(), nullptr /* parent_permissions_policy */,
            window->GetAgentClusterID(), ukm::kInvalidSourceId,
            window->GetExecutionContextToken()),
        std::nullopt, std::make_unique<WorkerDevToolsParams>());
    return thread;
  }

  void RunBasicTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::RunBasicTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunSimpleProcessTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::RunSimpleProcessTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunParsingTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::RunParsingTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunParsingParameterDescriptorTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::
                RunParsingParameterDescriptorTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

 private:
  void ExpectEvaluateScriptModule(AudioWorkletGlobalScope* global_scope,
                                  const String& source_code,
                                  bool expect_success) {
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    KURL js_url("https://example.com/worklet.js");
    v8::Local<v8::Module> module =
        ModuleTestBase::CompileModule(script_state, source_code, js_url);
    EXPECT_FALSE(module.IsEmpty());
    ScriptValue exception =
        ModuleRecord::Instantiate(script_state, module, js_url);
    EXPECT_TRUE(exception.IsEmpty());

    ScriptEvaluationResult result =
        JSModuleScript::CreateForTest(Modulator::From(script_state), module,
                                      js_url)
            ->RunScriptOnScriptStateAndReturnValue(script_state);
    if (expect_success) {
      EXPECT_FALSE(GetResult(script_state, std::move(result)).IsEmpty());
    } else {
      EXPECT_FALSE(GetException(script_state, std::move(result)).IsEmpty());
    }
  }

  // Test if AudioWorkletGlobalScope and V8 components (ScriptState, Isolate)
  // are properly instantiated. Runs a simple processor registration and check
  // if the class definition is correctly registered, then instantiate an
  // AudioWorkletProcessor instance from the definition.
  void RunBasicTestOnWorkletThread(WorkerThread* thread,
                                   base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());

    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);

    v8::Isolate* isolate = script_state->GetIsolate();
    EXPECT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            constructor () { super(); }
            process () {}
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    AudioWorkletProcessorDefinition* definition =
        global_scope->FindDefinition("testProcessor");
    EXPECT_TRUE(definition);
    EXPECT_EQ(definition->GetName(), "testProcessor");
    auto* channel = MakeGarbageCollected<MessageChannel>(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();

    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("testProcessor",
                                      dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);
    EXPECT_EQ(processor->Name(), "testProcessor");
    v8::Local<v8::Value> processor_value =
        ToV8Traits<AudioWorkletProcessor>::ToV8(script_state, processor);
    EXPECT_TRUE(processor_value->IsObject());

    wait_event->Signal();
  }

  // Test if various class definition patterns are parsed correctly.
  void RunParsingTestOnWorkletThread(WorkerThread* thread,
                                     base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());

    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);

    ScriptState::Scope scope(script_state);

    {
      // registerProcessor() with a valid class definition should define a
      // processor. Note that these classes will fail at the construction time
      // because they're not valid AudioWorkletProcessor.
      String source_code =
          R"JS(
            var class1 = function () {};
            class1.prototype.process = function () {};
            registerProcessor('class1', class1);

            var class2 = function () {};
            class2.prototype = { process: function () {} };
            registerProcessor('class2', class2);
          )JS";
      ExpectEvaluateScriptModule(global_scope, source_code, true);
      EXPECT_TRUE(global_scope->FindDefinition("class1"));
      EXPECT_TRUE(global_scope->FindDefinition("class2"));
    }

    {
      // registerProcessor() with an invalid class definition should fail to
      // define a processor.
      String source_code =
          R"JS(
            var class3 = function () {};
            Object.defineProperty(class3, 'prototype', {
                get: function () {
                  return {
                    process: function () {}
                  };
                }
              });
            registerProcessor('class3', class3);
          )JS";
      ExpectEvaluateScriptModule(global_scope, source_code, false);
      EXPECT_FALSE(global_scope->FindDefinition("class3"));
    }

    wait_event->Signal();
  }

  // Test if the invocation of process() method in AudioWorkletProcessor and
  // AudioWorkletGlobalScope is performed correctly.
  void RunSimpleProcessTestOnWorkletThread(WorkerThread* thread,
                                           base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    EXPECT_TRUE(isolate);
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            constructor () {
              super();
              this.constant_ = 1;
            }
            process (inputs, outputs) {
              let inputChannel = inputs[0][0];
              let outputChannel = outputs[0][0];
              for (let i = 0; i < outputChannel.length; ++i) {
                outputChannel[i] = inputChannel[i] + this.constant_;
              }
            }
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    auto* channel = MakeGarbageCollected<MessageChannel>(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("testProcessor",
                                      dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    Vector<scoped_refptr<AudioBus>> input_buses;
    Vector<scoped_refptr<AudioBus>> output_buses;
    HashMap<String, std::unique_ptr<AudioFloatArray>> param_data_map;
    scoped_refptr<AudioBus> input_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    AudioChannel* input_channel = input_bus->Channel(0);
    AudioChannel* output_channel = output_bus->Channel(0);

    input_buses.push_back(input_bus.get());
    output_buses.push_back(output_bus.get());

    // Fill `input_channel` with 1 and zero out `output_bus`.
    std::fill(input_channel->MutableData(),
              input_channel->MutableData() + input_channel->length(), 1);
    output_bus->Zero();

    // Then invoke the process() method to perform JS buffer manipulation. The
    // output buffer should contain a constant value of 2.
    processor->Process(input_buses, output_buses, param_data_map);
    for (unsigned i = 0; i < output_channel->length(); ++i) {
      EXPECT_EQ(output_channel->Data()[i], 2);
    }

    wait_event->Signal();
  }

  void RunParsingParameterDescriptorTestOnWorkletThread(
      WorkerThread* thread,
      base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            static get parameterDescriptors () {
              return [{
                name: 'gain',
                defaultValue: 0.707,
                minValue: 0.0,
                maxValue: 1.0
              }];
            }
            constructor () { super(); }
            process () {}
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    AudioWorkletProcessorDefinition* definition =
        global_scope->FindDefinition("testProcessor");
    EXPECT_TRUE(definition);
    EXPECT_EQ(definition->GetName(), "testProcessor");

    const Vector<String> param_names =
        definition->GetAudioParamDescriptorNames();
    EXPECT_EQ(param_names[0], "gain");

    const AudioParamDescriptor* descriptor =
        definition->GetAudioParamDescriptor(param_names[0]);
    EXPECT_EQ(descriptor->defaultValue(), 0.707f);
    EXPECT_EQ(descriptor->minValue(), 0.0f);
    EXPECT_EQ(descriptor->maxValue(), 1.0f);

    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AudioWorkletGlobalScopeTest, Basic) {
  std::unique_ptr<OfflineAudioWorkletThread> thread
      = CreateAudioWorkletThread();
  RunBasicTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, Parsing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread
      = CreateAudioWorkletThread();
  RunParsingTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, BufferProcessing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread
      = CreateAudioWorkletThread();
  RunSimpleProcessTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, ParsingParameterDescriptor) {
  std::unique_ptr<OfflineAudioWorkletThread> thread
      = CreateAudioWorkletThread();
  RunParsingParameterDescriptorTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

}  // namespace blink
