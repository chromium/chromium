// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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
#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_param_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_worklet_node_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_automation_rate.h"
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
#include "third_party/blink/renderer/modules/webaudio/audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_map.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

namespace {

constexpr size_t kRenderQuantumFrames = 128;
constexpr char kWorkletScriptUrl[] = "https://example.com/worklet.js";

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

  void RunDenormalProcessTest(WorkerThread* thread, bool expect_denormals) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::RunDenormalProcessTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            expect_denormals, CrossThreadUnretained(&waitable_event)));
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

  void RunAudioParamProcessTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletGlobalScopeTest::
                                RunAudioParamProcessTestOnWorkletThread,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(thread),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunAudioWorkletHandlerProcessTest(WorkerThread* thread) {
    DummyExceptionStateForTesting exception_state;
    OfflineAudioContext* context = OfflineAudioContext::Create(
        GetFrame().DomWindow(), 2, kRenderQuantumFrames, 48000,
        exception_state);
    ASSERT_FALSE(exception_state.HadException());

    AudioWorkletNodeOptions* options = AudioWorkletNodeOptions::Create();
    options->setNumberOfInputs(0);
    options->setNumberOfOutputs(1);
    options->setOutputChannelCount({2});

    auto create_descriptor = [](const String& name, V8AutomationRate::Enum rate,
                                float default_value) {
      AudioParamDescriptor* desc = AudioParamDescriptor::Create();
      desc->setName(name);
      desc->setAutomationRate(rate);
      desc->setDefaultValue(default_value);
      desc->setMinValue(-1000.0f);
      desc->setMaxValue(1000.0f);
      return CrossThreadAudioParamInfo(desc);
    };

    Vector<CrossThreadAudioParamInfo> param_info_list;
    param_info_list.push_back(
        create_descriptor("kRateParam", V8AutomationRate::Enum::kKRate, 5.0f));
    param_info_list.push_back(create_descriptor(
        "unautomatedARateParam", V8AutomationRate::Enum::kARate, 7.0f));
    param_info_list.push_back(create_descriptor(
        "constantARateParam", V8AutomationRate::Enum::kARate, 10.0f));
    param_info_list.push_back(create_descriptor(
        "varyingARateParam", V8AutomationRate::Enum::kARate, 20.0f));

    auto* node_channel =
        MakeGarbageCollected<MessageChannel>(GetFrame().DomWindow());
    AudioWorkletNode* worklet_node = MakeGarbageCollected<AudioWorkletNode>(
        *context, "handlerParamTestProcessor", options, param_info_list,
        node_channel->port1());
    worklet_node->connect(context->destinationNode(), 0, 0, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    const auto& param_map = worklet_node->parameters()->GetHashMap();
    // Schedule a ramp on `kRateParam` so `HasSampleAccurateValues()` is true,
    // verifying `k-rate` parameters still collapse to length 1.
    param_map.at("kRateParam")
        ->linearRampToValueAtTime(50.0f, 1.0, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // Schedule two events on `constantARateParam` so
    // `HasSampleAccurateValues()` is true while values across the first render
    // quantum remain constant at 10.0f, verifying `HasConstantValues()`
    // collapses the span to length 1.
    AudioParam* constant_a_rate = param_map.at("constantARateParam");
    constant_a_rate->setValueAtTime(10.0f, 0.0, exception_state);
    constant_a_rate->setValueAtTime(20.0f, 1.0, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // Schedule a linear ramp across the first render quantum on
    // `varyingARateParam` so values vary per frame and the span retains length
    // `kRenderQuantumFrames`.
    AudioParam* varying_a_rate = param_map.at("varyingARateParam");
    varying_a_rate->setValueAtTime(20.0f, 0.0, exception_state);
    varying_a_rate->linearRampToValueAtTime(
        20.0f + static_cast<float>(kRenderQuantumFrames),
        static_cast<double>(kRenderQuantumFrames) / 48000.0, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    scoped_refptr<AudioWorkletHandler> handler = WrapRefCounted(
        &static_cast<AudioWorkletHandler&>(worklet_node->Handler()));

    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::
                RunAudioWorkletHandlerProcessTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            WrapCrossThreadPersistent(context), handler,
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunProcessMethodUndefinedTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletGlobalScopeTest::
                                RunProcessMethodUndefinedTestOnWorkletThread,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(thread),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunProcessThrowingTest(WorkerThread* thread) {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletGlobalScopeTest::RunProcessThrowingTestOnWorkletThread,
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
    KURL js_url(kWorkletScriptUrl);
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
        global_scope->CreateProcessor("testProcessor", dummy_port_channel,
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
    V8DoNotRunMicrotasksScope microtasks_scope(script_state);

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
        global_scope->CreateProcessor("testProcessor", dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    Vector<scoped_refptr<AudioBus>> input_buses;
    Vector<scoped_refptr<AudioBus>> output_buses;
    HashMap<String, base::span<const float>> param_data_map;
    scoped_refptr<AudioBus> input_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    AudioChannel* input_channel = input_bus->Channel(0);
    AudioChannel* output_channel = output_bus->Channel(0);

    input_buses.push_back(input_bus.get());
    output_buses.push_back(output_bus.get());

    // Fill `input_channel` with 1 and zero out `output_bus`.
    std::ranges::fill(input_channel->MutableSpan(), 1);
    output_bus->Zero();

    // Then invoke the process() method to perform JS buffer manipulation. The
    // output buffer should contain a constant value of 2.
    processor->Process(input_buses, output_buses, param_data_map);
    for (unsigned i = 0; i < output_channel->length(); ++i) {
      EXPECT_EQ(output_channel->Span()[i], 2);
    }

    wait_event->Signal();
  }

  void RunDenormalProcessTestOnWorkletThread(WorkerThread* thread,
                                             bool expect_denormals,
                                             base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    EXPECT_TRUE(isolate);
    V8DoNotRunMicrotasksScope microtasks_scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            constructor () { super(); }
            process (inputs, outputs) {
              let f64 = new Float64Array(1);
              // The minimum positive normal 64-bit float is
              // 2.225e-308. Therefore, 1.0e-309 is a denormal
              // double. If FTZ/DAZ is enabled, it is treated
              // as zero or flushed to zero, making f64[0]
              // equal to 0.0. If disabled, the division
              // computes 1.0e-310 (a valid denormal double
              // > 0.0).
              let denorm = 1.0e-309;
              f64[0] = denorm / 10.0;
              let outputChannel = outputs[0][0];
              outputChannel[0] = f64[0] > 0.0 ? 1.0 : 0.0;
            }
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    auto* channel = MakeGarbageCollected<MessageChannel>(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("testProcessor", dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    Vector<scoped_refptr<AudioBus>> input_buses;
    Vector<scoped_refptr<AudioBus>> output_buses;
    HashMap<String, base::span<const float>> param_data_map;
    scoped_refptr<AudioBus> input_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    AudioChannel* output_channel = output_bus->Channel(0);

    input_buses.push_back(input_bus.get());
    output_buses.push_back(output_bus.get());
    output_bus->Zero();

    // Simulate the audio thread rendering stack by instantiating
    // DenormalDisabler.
    DenormalDisabler scoped_disabler;

    // processor->Process() internally instantiates DenormalEnabler, which
    // disables FTZ/DAZ during V8 execution if enabled.
    processor->Process(input_buses, output_buses, param_data_map);

    // Verify that the JS execution was affected by the outer
    // DenormalDisabler only if the feature is disabled.
    EXPECT_EQ(output_channel->Span()[0], expect_denormals ? 1.0f : 0.0f);

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

  void RunAudioParamProcessTestOnWorkletThread(
      WorkerThread* thread,
      base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class ParamTestProcessor extends AudioWorkletProcessor {
            static get parameterDescriptors() {
              return [
                { name: 'kRateParam', automationRate: 'k-rate' },
                { name: 'aRateParam', automationRate: 'a-rate' }
              ];
            }
            process(inputs, outputs, parameters) {
              const metaChannel = outputs[0][0];
              const valueChannel = outputs[0][1];
              metaChannel[0] = parameters.kRateParam.length;
              metaChannel[1] = parameters.kRateParam[0];
              metaChannel[2] = parameters.aRateParam.length;
              for (let i = 0; i < parameters.aRateParam.length; ++i) {
                valueChannel[i] = parameters.aRateParam[i];
              }
              return true;
            }
          }
          registerProcessor('paramTestProcessor', ParamTestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    auto* channel = MakeGarbageCollected<MessageChannel>(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("paramTestProcessor", dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    Vector<scoped_refptr<AudioBus>> input_buses;
    Vector<scoped_refptr<AudioBus>> output_buses;
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(2, kRenderQuantumFrames);
    output_bus->Zero();
    output_buses.push_back(output_bus.get());

    AudioFloatArray k_rate_array(1);
    k_rate_array[0] = 5.0f;

    AudioFloatArray a_rate_array(kRenderQuantumFrames);
    for (size_t i = 0; i < kRenderQuantumFrames; ++i) {
      a_rate_array[i] = 10.0f + static_cast<float>(i);
    }

    HashMap<String, base::span<const float>> param_data_map;
    param_data_map.Set("kRateParam", k_rate_array.as_span());
    param_data_map.Set("aRateParam", a_rate_array.as_span());

    // Quantum 1: kRateParam has length 1, aRateParam has length 128 with
    // non-constant values.
    processor->Process(input_buses, output_buses, param_data_map);

    AudioChannel* meta_channel = output_bus->Channel(0);
    AudioChannel* value_channel = output_bus->Channel(1);
    EXPECT_EQ(meta_channel->Span()[0], 1.0f);
    EXPECT_EQ(meta_channel->Span()[1], 5.0f);
    EXPECT_EQ(meta_channel->Span()[2],
              static_cast<float>(kRenderQuantumFrames));
    for (size_t i = 0; i < kRenderQuantumFrames; ++i) {
      EXPECT_EQ(value_channel->Span()[i], 10.0f + static_cast<float>(i));
    }

    // Quantum 2: Transition aRateParam from length 128 to length 1, exercising
    // ParamValueMapMatchesToParamsObject -> CloneParamValueMapToObject.
    output_bus->Zero();
    a_rate_array[0] = 42.0f;
    param_data_map.Set("aRateParam", a_rate_array.as_span().first(1u));
    processor->Process(input_buses, output_buses, param_data_map);

    EXPECT_EQ(meta_channel->Span()[0], 1.0f);
    EXPECT_EQ(meta_channel->Span()[1], 5.0f);
    EXPECT_EQ(meta_channel->Span()[2], 1.0f);
    EXPECT_EQ(value_channel->Span()[0], 42.0f);

    // Quantum 3: Transition aRateParam back from length 1 to 128.
    output_bus->Zero();
    for (size_t i = 0; i < kRenderQuantumFrames; ++i) {
      a_rate_array[i] = 100.0f + static_cast<float>(i);
    }
    param_data_map.Set("aRateParam", a_rate_array.as_span());
    processor->Process(input_buses, output_buses, param_data_map);

    EXPECT_EQ(meta_channel->Span()[2],
              static_cast<float>(kRenderQuantumFrames));
    for (size_t i = 0; i < kRenderQuantumFrames; ++i) {
      EXPECT_EQ(value_channel->Span()[i], 100.0f + static_cast<float>(i));
    }

    wait_event->Signal();
  }

  void RunAudioWorkletHandlerProcessTestOnWorkletThread(
      WorkerThread* thread,
      OfflineAudioContext* context,
      scoped_refptr<AudioWorkletHandler> handler,
      base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class HandlerParamTestProcessor extends AudioWorkletProcessor {
            static get parameterDescriptors() {
              return [
                { name: 'kRateParam', automationRate: 'k-rate' },
                { name: 'unautomatedARateParam', automationRate: 'a-rate' },
                { name: 'constantARateParam', automationRate: 'a-rate' },
                { name: 'varyingARateParam', automationRate: 'a-rate' }
              ];
            }
            process(inputs, outputs, parameters) {
              const metaChannel = outputs[0][0];
              const valueChannel = outputs[0][1];
              metaChannel[0] = parameters.kRateParam.length;
              metaChannel[1] = parameters.kRateParam[0];
              metaChannel[2] = parameters.unautomatedARateParam.length;
              metaChannel[3] = parameters.unautomatedARateParam[0];
              metaChannel[4] = parameters.constantARateParam.length;
              metaChannel[5] = parameters.constantARateParam[0];
              metaChannel[6] = parameters.varyingARateParam.length;
              for (let i = 0; i < parameters.varyingARateParam.length; ++i) {
                valueChannel[i] = parameters.varyingARateParam[i];
              }
              return false;
            }
          }
          registerProcessor(
              'handlerParamTestProcessor', HandlerParamTestProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    auto* channel = MakeGarbageCollected<MessageChannel>(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor = global_scope->CreateProcessor(
        "handlerParamTestProcessor", dummy_port_channel,
        SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    DeferredTaskHandler& deferred_task_handler =
        context->GetDeferredTaskHandler();
    deferred_task_handler.SetAudioThreadToCurrentThread();
    {
      DeferredTaskHandler::GraphAutoLocker locker(deferred_task_handler);
      deferred_task_handler.HandleDeferredTasks();
      handler->Output(0).UpdateRenderingState();
    }

    handler->SetProcessorOnRenderThread(processor);
    handler->Output(0).Bus()->Zero();
    handler->Process(kRenderQuantumFrames);

    AudioChannel* meta_channel = handler->Output(0).Bus()->Channel(0);
    AudioChannel* value_channel = handler->Output(0).Bus()->Channel(1);

    // k-rate parameter with active ramp automation -> length 1, value 5.0f.
    EXPECT_EQ(meta_channel->Span()[0], 1.0f);
    EXPECT_FLOAT_EQ(meta_channel->Span()[1], 5.0f);

    // a-rate parameter without automation -> length 1, value 7.0f.
    EXPECT_EQ(meta_channel->Span()[2], 1.0f);
    EXPECT_FLOAT_EQ(meta_channel->Span()[3], 7.0f);

    // a-rate parameter with sample-accurate automation that is constant across
    // the render quantum -> collapsed to length 1, value 10.0f.
    EXPECT_EQ(meta_channel->Span()[4], 1.0f);
    EXPECT_FLOAT_EQ(meta_channel->Span()[5], 10.0f);

    // a-rate parameter with sample-accurate linear ramp across the render
    // quantum -> length kRenderQuantumFrames (128), values 20.0f .. 147.0f.
    EXPECT_EQ(meta_channel->Span()[6],
              static_cast<float>(kRenderQuantumFrames));
    for (size_t i = 0; i < kRenderQuantumFrames; ++i) {
      EXPECT_NEAR(value_channel->Span()[i], 20.0f + static_cast<float>(i),
                  1e-3f);
    }

    wait_event->Signal();
  }

  AudioWorkletProcessor* CreateAndProcessProcessor(
      AudioWorkletGlobalScope* global_scope,
      const String& name) {
    Vector<scoped_refptr<AudioBus>> input_buses;
    Vector<scoped_refptr<AudioBus>> output_buses;
    HashMap<String, base::span<const float>> param_data_map;
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    output_bus->Zero();
    output_buses.push_back(output_bus);

    auto* channel = MakeGarbageCollected<MessageChannel>(global_scope);
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor(name, std::move(dummy_port_channel),
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);
    EXPECT_FALSE(processor->Process(input_buses, output_buses, param_data_map));
    EXPECT_TRUE(processor->hasErrorOccurred());
    return processor;
  }

  // Verifies error details reported when `process` is non-callable: missing,
  // data property, or getter returning a non-callable value reports
  // kProcessMethodUndefinedError with a synthetic TypeError message and no
  // source location.
  void RunProcessMethodUndefinedTestOnWorkletThread(
      WorkerThread* thread,
      base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ScriptState::Scope scope(script_state);
    V8DoNotRunMicrotasksScope microtasks_scope(script_state);

    String source_code =
        R"JS(
          class UndefinedProcessProcessor extends AudioWorkletProcessor {}
          registerProcessor('undefinedProcess', UndefinedProcessProcessor);

          class NonCallableDataProcessor extends AudioWorkletProcessor {
            constructor() {
              super();
              this.process = 42;
            }
          }
          registerProcessor('nonCallableData', NonCallableDataProcessor);

          class NonCallableGetterProcessor extends AudioWorkletProcessor {
            get process() {
              return 'not a function';
            }
          }
          registerProcessor('nonCallableGetter', NonCallableGetterProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    for (const char* name :
         {"undefinedProcess", "nonCallableData", "nonCallableGetter"}) {
      SCOPED_TRACE(name);
      AudioWorkletProcessor* processor =
          CreateAndProcessProcessor(global_scope, name);
      const AudioWorkletProcessorErrorDetails& details =
          processor->GetErrorDetails();
      EXPECT_EQ(details.error_state,
                AudioWorkletProcessorErrorState::kProcessMethodUndefinedError);
      EXPECT_TRUE(details.error_message.starts_with("TypeError"));
      EXPECT_TRUE(details.source_url.empty());
      EXPECT_EQ(details.line_number, 0);
      EXPECT_EQ(details.column_number, 0);
    }

    wait_event->Signal();
  }

  // Verifies error details reported when `process` throws an exception: an
  // exception thrown by the `process` getter or by process() itself reports
  // kProcessError with the exception's own message and location.
  void RunProcessThrowingTestOnWorkletThread(WorkerThread* thread,
                                             base::WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = To<AudioWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ScriptState::Scope scope(script_state);
    V8DoNotRunMicrotasksScope microtasks_scope(script_state);

    String source_code =
        R"JS(
          class ThrowingGetterProcessor extends AudioWorkletProcessor {
            get process() {
              throw new Error('process getter threw an error');
            }
          }
          registerProcessor('throwingGetter', ThrowingGetterProcessor);

          class ThrowingProcessProcessor extends AudioWorkletProcessor {
            process() {
              throw new Error('process threw an error');
            }
          }
          registerProcessor('throwingProcess', ThrowingProcessProcessor);
        )JS";
    ExpectEvaluateScriptModule(global_scope, source_code, true);

    for (const char* name : {"throwingGetter", "throwingProcess"}) {
      SCOPED_TRACE(name);
      AudioWorkletProcessor* processor =
          CreateAndProcessProcessor(global_scope, name);
      const AudioWorkletProcessorErrorDetails& details =
          processor->GetErrorDetails();
      EXPECT_EQ(details.error_state,
                AudioWorkletProcessorErrorState::kProcessError);
      EXPECT_FALSE(details.error_message.empty());
      EXPECT_EQ(details.source_url, kWorkletScriptUrl);
      EXPECT_GT(details.line_number, 0);
      EXPECT_GT(details.column_number, 0);
    }

    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AudioWorkletGlobalScopeTest, Basic) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunBasicTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, Parsing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunParsingTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, BufferProcessing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunSimpleProcessTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, DenormalProcessing_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kAudioWorkletJSDenormalEnabler);

  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunDenormalProcessTest(thread.get(), /*expect_denormals=*/true);
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, DenormalProcessing_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      blink::features::kAudioWorkletJSDenormalEnabler);

  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunDenormalProcessTest(thread.get(), /*expect_denormals=*/false);
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, ParsingParameterDescriptor) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunParsingParameterDescriptorTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, AudioParamProcessing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunAudioParamProcessTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, AudioWorkletHandlerParamSizing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunAudioWorkletHandlerProcessTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, ProcessMethodUndefined) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunProcessMethodUndefinedTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, ProcessThrowing) {
  std::unique_ptr<OfflineAudioWorkletThread> thread =
      CreateAudioWorkletThread();
  RunProcessThrowingTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

}  // namespace blink
