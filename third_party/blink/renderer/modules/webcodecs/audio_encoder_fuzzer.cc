// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/mojo_audio_encoder_service.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "base/win/scoped_com_initializer.h"
#include "media/gpu/windows/mf_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif

#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif

#if HAS_AAC_ENCODER
namespace {

// Other end of remote InterfaceFactory requested by AudioEncoder. Used
// to create real media::mojom::AudioEncoders.
class TestInterfaceFactory : public media::mojom::InterfaceFactory {
 public:
  TestInterfaceFactory() = default;
  ~TestInterfaceFactory() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory>(
        std::move(handle)));

    // Each AudioEncoder instance will try to open a connection to this
    // factory, so we must clean up after each one is destroyed.
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &TestInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // Implement this one interface from mojom::InterfaceFactory.
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) override {
    // While we'd like to use the real GpuMojoMediaFactory here, it requires
    // quite a bit more of scaffolding to setup and isn't really needed.
#if BUILDFLAG(IS_MAC)
    auto platform_audio_encoder =
        std::make_unique<media::AudioToolboxAudioEncoder>();
#elif BUILDFLAG(IS_WIN)
    CHECK(com_initializer_.Succeeded());
    auto platform_audio_encoder = std::make_unique<media::MFAudioEncoder>(
        blink::scheduler::GetSequencedTaskRunnerForTesting());
#else
#error "Unknown platform encoder."
#endif
    audio_encoder_receivers_.Add(
        std::make_unique<media::MojoAudioEncoderService>(
            std::move(platform_audio_encoder)),
        std::move(receiver));
  }

  // Stub out other mojom::InterfaceFactory interfaces.
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) override {}
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {}
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) override {}
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif  // BUILDFLAG(IS_ANDROID)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) override {
    std::move(callback).Run(mojo::NullRemote(), nullptr,
                            media::CreateCdmStatus::kCdmNotSupported);
  }

#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<
          ::media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) override {}
#endif  // BUILDFLAG(IS_WIN)
 private:
#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif  // BUILDFLAG(IS_WIN)
  // media::MojoCdmServiceContext cdm_service_context_;
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
  mojo::UniqueReceiverSet<media::mojom::AudioEncoder> audio_encoder_receivers_;
};

}  // namespace
#endif  // HAS_AAC_ENCODER

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::AudioEncoderApiInvocationSequence& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);

#if HAS_AAC_ENCODER
  base::test::ScopedFeatureList platform_aac(media::kPlatformAudioEncoder);
  static const bool kSetTestBinder = []() {
    auto interface_factory = std::make_unique<TestInterfaceFactory>();
    return Platform::Current()
        ->GetBrowserInterfaceBroker()
        ->SetBinderForTesting(
            media::mojom::InterfaceFactory::Name_,
            WTF::BindRepeating(&TestInterfaceFactory::BindRequest,
                               base::Owned(std::move(interface_factory))));
  }();
  CHECK(kSetTestBinder) << "Failed to register media interface binder.";
#endif

  //
  // NOTE: GC objects that need to survive iterations of the loop below
  // must be Persistent<>!
  //
  // GC may be triggered by the RunLoop().RunUntilIdle() below, which will GC
  // raw pointers on the stack. This is not required in production code because
  // GC typically runs at the top of the stack, or is conservative enough to
  // keep stack pointers alive.
  //

  // Scoping Persistent<> refs so GC can collect these at the end.
  Persistent<ScriptState> script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  ScriptState::Scope scope(script_state);

  Persistent<ScriptFunction> error_function =
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<FakeFunction>("error"));
  Persistent<V8WebCodecsErrorCallback> error_callback =
      V8WebCodecsErrorCallback::Create(error_function->V8Function());
  Persistent<ScriptFunction> output_function =
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<FakeFunction>("output"));
  Persistent<V8EncodedAudioChunkOutputCallback> output_callback =
      V8EncodedAudioChunkOutputCallback::Create(output_function->V8Function());

  Persistent<AudioEncoderInit> audio_encoder_init =
      MakeGarbageCollected<AudioEncoderInit>();
  audio_encoder_init->setError(error_callback);
  audio_encoder_init->setOutput(output_callback);

  Persistent<AudioEncoder> audio_encoder = AudioEncoder::Create(
      script_state, audio_encoder_init, IGNORE_EXCEPTION_FOR_TESTING);

  if (audio_encoder) {
    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::AudioEncoderApiInvocation::kConfigure: {
          AudioEncoderConfig* config =
              MakeAudioEncoderConfig(invocation.configure());

          // Use the same config to fuzz isConfigSupported().
          AudioEncoder::isConfigSupported(script_state, config,
                                          IGNORE_EXCEPTION_FOR_TESTING);

          audio_encoder->configure(config, IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::AudioEncoderApiInvocation::kEncode: {
          AudioData* data =
              MakeAudioData(script_state, invocation.encode().data());
          if (!data) {
            return;
          }

          audio_encoder->encode(data, IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::AudioEncoderApiInvocation::kFlush: {
          // TODO(https://crbug.com/1119253): Fuzz whether to await resolution
          // of the flush promise.
          audio_encoder->flush(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::AudioEncoderApiInvocation::kReset:
          audio_encoder->reset(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioEncoderApiInvocation::kClose:
          audio_encoder->close(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioEncoderApiInvocation::API_NOT_SET:
          break;
      }

      // Give other tasks a chance to run (e.g. calling our output callback).
      base::RunLoop().RunUntilIdle();
    }
  }
}

}  // namespace blink
