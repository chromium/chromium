// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_key_system_track_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_decoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_encoding_configuration.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/media_capabilities/fuzzer_media_configuration.pb.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace blink {

String MediaKeysRequirementToString(
    mc_fuzzer::MediaConfigProto_KeySystemConfig_MediaKeysRequirement
        proto_requirement) {
  switch (proto_requirement) {
    case mc_fuzzer::
        MediaConfigProto_KeySystemConfig_MediaKeysRequirement_REQUIRED:
      return "required";
    case mc_fuzzer::
        MediaConfigProto_KeySystemConfig_MediaKeysRequirement_NOT_REQUIRED:
      return "optional";
    case mc_fuzzer::
        MediaConfigProto_KeySystemConfig_MediaKeysRequirement_NOT_ALLOWED:
      return "not-allowed";
  }
  return "";
}

Vector<String> MediaSessionTypeToVector(
    const ::google::protobuf::RepeatedField<int>& proto_session_types) {
  Vector<String> result;
  for (auto& proto_session_type : proto_session_types) {
    String session_type;
    switch (proto_session_type) {
      case mc_fuzzer::
          MediaConfigProto_KeySystemConfig_MediaKeySessionType_TEMPORARY:
        session_type = "temporary";
        break;
      case mc_fuzzer::
          MediaConfigProto_KeySystemConfig_MediaKeySessionType_PERSISTENT_LICENSE:
        session_type = "persistent-license";
        break;
    }
    result.push_back(session_type);
  }
  return result;
}

template <class T>
T* MakeConfiguration(const mc_fuzzer::MediaConfigProto& proto) {
  Persistent<T> config = T::Create();
  if (proto.has_video()) {
    config->setVideo(VideoConfiguration::Create());
    config->video()->setContentType(proto.video().content_type().c_str());
    config->video()->setWidth(proto.video().width());
    config->video()->setHeight(proto.video().height());
    config->video()->setBitrate(proto.video().bitrate());
    config->video()->setFramerate(proto.video().framerate());
    config->video()->setSpatialScalability(proto.video().spatial_scalability());
    config->video()->setScalabilityMode(
        proto.video().scalability_mode().c_str());
  }

  if (proto.has_audio()) {
    config->setAudio(AudioConfiguration::Create());
    config->audio()->setContentType(proto.audio().content_type().c_str());
    config->audio()->setChannels(proto.audio().channels().c_str());
    config->audio()->setBitrate(proto.audio().bitrate());
    config->audio()->setSamplerate(proto.audio().samplerate());
  }

  switch (proto.type()) {
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_FILE:
      config->setType("file");
      break;
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_MEDIA_SOURCE:
      config->setType("media-source");
      break;
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_WEBRTC:
    case mc_fuzzer::MediaConfigProto_MediaType_ENCODING_WEBRTC:
      config->setType("webrtc");
      break;
  }
  return config;
}

void AddDecodingSpecificConfiguration(const mc_fuzzer::MediaConfigProto& proto,
                                      MediaDecodingConfiguration* config) {
  if (proto.has_key_system_config()) {
    config->setKeySystemConfiguration(
        MediaCapabilitiesKeySystemConfiguration::Create());
    config->keySystemConfiguration()->setKeySystem(
        String::FromUTF8(proto.key_system_config().key_system().c_str()));
    config->keySystemConfiguration()->setInitDataType(
        String::FromUTF8(proto.key_system_config().init_data_type().c_str()));
    config->keySystemConfiguration()->setDistinctiveIdentifier(
        MediaKeysRequirementToString(
            proto.key_system_config().distinctive_identifier()));
    config->keySystemConfiguration()->setPersistentState(
        MediaKeysRequirementToString(
            proto.key_system_config().persistent_state()));
    config->keySystemConfiguration()->setSessionTypes(
        MediaSessionTypeToVector(proto.key_system_config().session_types()));

    if (proto.key_system_config().has_key_system_audio_config()) {
      config->keySystemConfiguration()->setAudio(
          KeySystemTrackConfiguration::Create());
      config->keySystemConfiguration()->audio()->setRobustness(
          String::FromUTF8(proto.key_system_config()
                               .key_system_audio_config()
                               .robustness()
                               .c_str()));
    }
    if (proto.key_system_config().has_key_system_video_config()) {
      config->keySystemConfiguration()->setVideo(
          KeySystemTrackConfiguration::Create());
      config->keySystemConfiguration()->video()->setRobustness(
          String::FromUTF8(proto.key_system_config()
                               .key_system_video_config()
                               .robustness()
                               .c_str()));
    }
  }
}

DEFINE_TEXT_PROTO_FUZZER(const mc_fuzzer::MediaConfigProto& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);

  ScriptState* script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  ScriptState::Scope scope(script_state);

  auto* media_capabilities = MediaCapabilities::mediaCapabilities(
      *page_holder->GetFrame().DomWindow()->navigator());

  switch (proto.type()) {
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_FILE:
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_MEDIA_SOURCE:
    case mc_fuzzer::MediaConfigProto_MediaType_DECODING_WEBRTC: {
      auto* config = MakeConfiguration<MediaDecodingConfiguration>(proto);
      AddDecodingSpecificConfiguration(proto, config);
      media_capabilities->decodingInfo(script_state, config,
                                       IGNORE_EXCEPTION_FOR_TESTING);
    } break;
    case mc_fuzzer::MediaConfigProto_MediaType_ENCODING_WEBRTC: {
      auto* config = MakeConfiguration<MediaEncodingConfiguration>(proto);
      media_capabilities->encodingInfo(script_state, config,
                                       IGNORE_EXCEPTION_FOR_TESTING);
    } break;
  }

  // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
  //
  // Multiple GCs may be required to ensure everything is collected (due to
  // a chain of persistent handles), so some objects may not be collected until
  // a subsequent iteration. This is slow enough as is, so we compromise on one
  // major GC, as opposed to the 5 used in V8GCController for unit tests.
  script_state->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
