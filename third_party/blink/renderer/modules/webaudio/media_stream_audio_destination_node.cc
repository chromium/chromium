/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"

#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_node_options.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_handler.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

// Default to stereo; `options` will update it appropriately if needed.
constexpr uint32_t kDefaultNumberOfChannels = 2;

MediaStreamSource* CreateMediaStreamSource(
    ExecutionContext* execution_context) {
  DVLOG(1) << "Creating WebAudio media stream source.";
  auto audio_source = std::make_unique<WebAudioMediaStreamSource>(
      execution_context->GetTaskRunner(TaskType::kInternalMedia));
  WebAudioMediaStreamSource* audio_source_ptr = audio_source.get();

  String source_id = "WebAudio-" + WTF::CreateCanonicalUUIDString();

  MediaStreamSource::Capabilities capabilities;
  capabilities.device_id = source_id;
  capabilities.echo_cancellation = Vector<bool>({false});
  capabilities.auto_gain_control = Vector<bool>({false});
  capabilities.noise_suppression = Vector<bool>({false});
  capabilities.voice_isolation = Vector<bool>({false});
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };

  auto* source = MakeGarbageCollected<MediaStreamSource>(
      source_id, MediaStreamSource::kTypeAudio,
      "MediaStreamAudioDestinationNode", false, std::move(audio_source),
      MediaStreamSource::kReadyStateLive, true);
  audio_source_ptr->SetMediaStreamSource(source);
  source->SetCapabilities(capabilities);
  return source;
}

}  // namespace

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(
    AudioContext& context,
    uint32_t number_of_channels)
    : AudioNode(context),
      source_(CreateMediaStreamSource(context.GetExecutionContext())),
      stream_(MediaStream::Create(
          context.GetExecutionContext(),
          MediaStreamTrackVector({MediaStreamUtils::CreateLocalAudioTrack(
              context.GetExecutionContext(),
              source_)}))) {
  SetHandler(
      MediaStreamAudioDestinationHandler::Create(*this, number_of_channels));
  SendLogMessage(
      __func__, String::Format(
                    "({context.state=%s}, {context.sampleRate=%.0f}, "
                    "{number_of_channels=%u}, {handler=0x%" PRIXPTR
                    "}, [this=0x%" PRIXPTR "])",
                    context.state().Utf8().c_str(), context.sampleRate(),
                    number_of_channels, reinterpret_cast<uintptr_t>(&Handler()),
                    reinterpret_cast<uintptr_t>(this)));
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext& context,
    uint32_t number_of_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<MediaStreamAudioDestinationNode>(
      context, number_of_channels);
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext* context,
    const AudioNodeOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!context->CheckExecutionContextAndThrowIfNecessary(exception_state)) {
    return nullptr;
  }
  MediaStreamAudioDestinationNode* node =
      MakeGarbageCollected<MediaStreamAudioDestinationNode>(
          *context, kDefaultNumberOfChannels);

  // Need to handle channelCount here ourselves because the upper
  // limit is different from the normal AudioNode::setChannelCount
  // limit of 32.  Error messages will sometimes show the wrong
  // limits.
  if (options->hasChannelCount()) {
    node->setChannelCount(options->channelCount(), exception_state);
  }

  node->HandleChannelOptions(options, exception_state);

  return node;
}

void MediaStreamAudioDestinationNode::Trace(Visitor* visitor) const {
  visitor->Trace(stream_);
  visitor->Trace(source_);
  AudioNode::Trace(visitor);
}

void MediaStreamAudioDestinationNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaStreamAudioDestinationNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

void MediaStreamAudioDestinationNode::SendLogMessage(
    const char* const function_name,
    const String& message) {
  WebRtcLogMessage(
      String::Format("[WA]MSADN::%s %s", function_name, message.Utf8().c_str())
          .Utf8());
}

}  // namespace blink
