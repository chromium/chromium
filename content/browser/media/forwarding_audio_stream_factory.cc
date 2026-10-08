// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/forwarding_audio_stream_factory.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/guest_page_holder_impl.h"
#include "content/browser/media/audio_output_stream_broker.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

ForwardingAudioStreamFactory::AudioStreamFactoryBinder&
GetAudioStreamFactoryBinderOverride() {
  static base::NoDestructor<
      ForwardingAudioStreamFactory::AudioStreamFactoryBinder>
      binder;
  return *binder;
}

void BindStreamFactoryFromUIThread(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  const auto& binder_override = GetAudioStreamFactoryBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  GetAudioService().BindStreamFactory(std::move(receiver));
}

}  // namespace

ForwardingAudioStreamFactory::Core::Core(
    base::WeakPtr<ForwardingAudioStreamFactory> owner,
    std::unique_ptr<AudioStreamBrokerFactory> broker_factory)
    : owner_(std::move(owner)),
      broker_factory_(std::move(broker_factory)),
      group_id_(base::UnguessableToken::Create()) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  CHECK(owner_, base::NotFatalUntil::M160);
  CHECK(broker_factory_, base::NotFatalUntil::M160);
}

ForwardingAudioStreamFactory::Core::~Core() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  for (AudioStreamBroker::LoopbackSink* sink : loopback_sinks_) {
    sink->OnSourceGone();
  }
}

base::WeakPtr<ForwardingAudioStreamFactory::Core>
ForwardingAudioStreamFactory::Core::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ForwardingAudioStreamFactory::Core::CreateInputStream(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool enable_agc,
    media::mojom::AudioProcessingConfigPtr processing_config,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        renderer_factory_client) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);

  // |this| owns |inputs_|, so Unretained is safe.
  inputs_
      .insert(broker_factory_->CreateAudioInputStreamBroker(
          render_process_id, render_frame_id, device_id, params, group_id_,
          shared_memory_count, enable_agc, std::move(processing_config),
          base::BindOnce(&ForwardingAudioStreamFactory::Core::RemoveInput,
                         base::Unretained(this)),
          std::move(renderer_factory_client)))
      .first->get()
      ->CreateStream(GetFactory());
}

void ForwardingAudioStreamFactory::Core::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& raw_output_device_id) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  // Avoid spawning a factory if this for some reason gets called with an
  // invalid |input_stream_id| before any streams are created.
  if (!inputs_.empty()) {
    GetFactory()->AssociateInputAndOutputForAec(input_stream_id,
                                                raw_output_device_id);
  }
}

void ForwardingAudioStreamFactory::Core::CreateOutputStream(
    int render_process_id,
    int render_frame_id,
    const GlobalRenderFrameHostToken& main_frame_token,
    const std::string& device_id,
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);

  // |this| owns |outputs_|, so Unretained is safe.
  outputs_
      .insert(broker_factory_->CreateAudioOutputStreamBroker(
          render_process_id, render_frame_id, main_frame_token,
          ++stream_id_counter_, device_id, params, group_id_,
          base::BindOnce(&ForwardingAudioStreamFactory::Core::RemoveOutput,
                         base::Unretained(this)),
          std::move(client)))
      .first->get()
      ->CreateStream(GetFactory());
}

void ForwardingAudioStreamFactory::Core::CreateLoopbackStream(
    int render_process_id,
    int render_frame_id,
    AudioStreamBroker::LoopbackSource* loopback_source,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool mute_source,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        renderer_factory_client) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  CHECK(loopback_source, base::NotFatalUntil::M160);

  TRACE_EVENT_BEGIN("audio", "CreateLoopbackStream", "group",
                    group_id_.GetLowForSerialization());

  // |this| owns |inputs_|, so Unretained is safe.
  inputs_
      .insert(broker_factory_->CreateAudioLoopbackStreamBroker(
          render_process_id, render_frame_id, loopback_source, params,
          shared_memory_count, mute_source,
          base::BindOnce(&ForwardingAudioStreamFactory::Core::RemoveInput,
                         base::Unretained(this)),
          std::move(renderer_factory_client)))
      .first->get()
      ->CreateStream(GetFactory());
  TRACE_EVENT_END("audio", "source",
                  loopback_source->GetGroupID().GetLowForSerialization());
}

void ForwardingAudioStreamFactory::Core::SetMuted(bool muted) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  CHECK_NE(muted, !!muter_, base::NotFatalUntil::M160);
  TRACE_EVENT_INSTANT("audio", "SetMuted", "group",
                      group_id_.GetLowForSerialization(), "muted", muted);

  if (!muted) {
    muter_.reset();
    return;
  }

  muter_.emplace(group_id_);
  if (remote_factory_) {
    muter_->Connect(remote_factory_.get());
  }
}

void ForwardingAudioStreamFactory::Core::AddLoopbackSink(
    AudioStreamBroker::LoopbackSink* sink) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  loopback_sinks_.insert(sink);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ForwardingAudioStreamFactory::LoopbackStreamStarted,
                     owner_));
}

void ForwardingAudioStreamFactory::Core::RemoveLoopbackSink(
    AudioStreamBroker::LoopbackSink* sink) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  loopback_sinks_.erase(sink);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ForwardingAudioStreamFactory::LoopbackStreamStopped,
                     owner_));
}

const base::UnguessableToken& ForwardingAudioStreamFactory::Core::GetGroupID() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  return group_id();
}

// static
ForwardingAudioStreamFactory* ForwardingAudioStreamFactory::ForFrame(
    RenderFrameHost* frame) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!frame) {
    return nullptr;
  }

  GuestPageHolderImpl* guest_holder = GuestPageHolderImpl::FromRenderFrameHost(
      static_cast<RenderFrameHostImpl&>(*frame));
  if (guest_holder) {
    return guest_holder->GetAudioStreamFactory();
  }

  auto* contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));
  if (!contents) {
    return nullptr;
  }

  return contents->GetAudioStreamFactory();
}

// static
ForwardingAudioStreamFactory::Core* ForwardingAudioStreamFactory::CoreForFrame(
    RenderFrameHost* frame) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  ForwardingAudioStreamFactory* forwarding_factory =
      ForwardingAudioStreamFactory::ForFrame(frame);
  return forwarding_factory ? forwarding_factory->core() : nullptr;
}

ForwardingAudioStreamFactory::ForwardingAudioStreamFactory(
    WebContents* web_contents,
    std::unique_ptr<AudioStreamBrokerFactory> broker_factory)
    : WebContentsObserver(web_contents), core_() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  core_ = std::make_unique<Core>(weak_ptr_factory_.GetWeakPtr(),
                                 std::move(broker_factory));
}

ForwardingAudioStreamFactory::~ForwardingAudioStreamFactory() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(core_)));
}

void ForwardingAudioStreamFactory::LoopbackStreamStarted() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  capture_handle_ = web_contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
}

void ForwardingAudioStreamFactory::LoopbackStreamStopped() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  capture_handle_.RunAndReset();
}

void ForwardingAudioStreamFactory::SetMuted(bool muted) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (is_muted_ != muted) {
    is_muted_ = muted;

    // Unretained is safe since the destruction of |core_| will be posted to the
    // IO thread later.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::SetMuted, base::Unretained(core_.get()), muted));
  }
}

bool ForwardingAudioStreamFactory::IsMuted() const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  return is_muted_;
}

void ForwardingAudioStreamFactory::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  CHECK(render_frame_host, base::NotFatalUntil::M160);

  // Default-device audio output streams in a renderer process are shared across
  // all frames belonging to the same main frame via AudioRendererMixerManager.
  // When the main frame is in the same process, AudioRendererMixerManager
  // already binds shared mixer sinks directly to the main frame. However, when
  // the main frame is remote to those subframes' process, the mixer sink is
  // initially created through whichever subframe requested audio first. If that
  // subframe is deleted while other subframes in the same process and main
  // frame remain alive, reparent the shared stream broker to a surviving frame
  // instead of tearing down audio for the remaining subframes.
  //
  // Note: This reparents all mixable default-device output brokers of the
  // deleted frame regardless of whether the sink is currently active/playing
  // (since a surviving subframe may hold a paused mixer input that will later
  // resume playback on the shared sink). If the deleted frame was the only
  // consumer of the sink, the renderer will close the sink once its input is
  // removed, bounding the broker's lifetime.
  std::optional<int> fallback_render_frame_id;
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  RenderProcessHost* process = render_frame_host->GetProcess();
  if (render_frame_host != main_frame && main_frame->GetProcess() != process) {
    main_frame->ForEachRenderFrameHostWithAction(
        [render_frame_host, main_frame, process,
         &fallback_render_frame_id](RenderFrameHost* rfh) {
          if (rfh->GetMainFrame() != main_frame) {
            return RenderFrameHost::FrameIterationAction::kSkipChildren;
          }
          if (rfh != render_frame_host && rfh->IsRenderFrameLive() &&
              rfh->IsActive() && rfh->GetProcess() == process) {
            fallback_render_frame_id = rfh->GetRoutingID();
            return RenderFrameHost::FrameIterationAction::kStop;
          }
          return RenderFrameHost::FrameIterationAction::kContinue;
        });
  }

  // Unretained is safe since the destruction of |core_| will be posted to the
  // IO thread later.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::CleanupOrReparentStreamsBelongingTo,
                     base::Unretained(core_.get()),
                     render_frame_host->GetProcess()->GetDeprecatedID(),
                     render_frame_host->GetRoutingID(),
                     fallback_render_frame_id));
}

void ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
    AudioStreamFactoryBinder binder) {
  GetAudioStreamFactoryBinderOverride() = std::move(binder);
}

void ForwardingAudioStreamFactory::Core::CleanupOrReparentStreamsBelongingTo(
    int render_process_id,
    int render_frame_id,
    std::optional<int> fallback_render_frame_id) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);

  TRACE_EVENT_BEGIN("audio", "CleanupOrReparentStreamsBelongingTo", "group",
                    group_id_.GetLowForSerialization(), "process id",
                    render_process_id);

  auto match_rfh =
      [render_process_id, render_frame_id](
          const std::unique_ptr<AudioStreamBroker>& broker) -> bool {
    return broker->render_process_id() == render_process_id &&
           broker->render_frame_id() == render_frame_id;
  };

  auto match_or_reparent_output =
      [render_process_id, render_frame_id, fallback_render_frame_id](
          const std::unique_ptr<AudioStreamBroker>& broker) -> bool {
    if (broker->render_process_id() != render_process_id ||
        broker->render_frame_id() != render_frame_id) {
      return false;
    }
    if (fallback_render_frame_id.has_value() &&
        static_cast<AudioOutputStreamBroker*>(broker.get())
            ->Reparent(*fallback_render_frame_id)) {
      return false;
    }
    return true;
  };

  base::EraseIf(outputs_, match_or_reparent_output);
  base::EraseIf(inputs_, match_rfh);

  ResetRemoteFactoryPtrIfIdle();

  TRACE_EVENT_END("audio", "frame_id", render_frame_id);
}

void ForwardingAudioStreamFactory::Core::RemoveInput(
    AudioStreamBroker* broker) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  size_t removed = inputs_.erase(broker);
  CHECK_EQ(1u, removed, base::NotFatalUntil::M160);

  ResetRemoteFactoryPtrIfIdle();
}

void ForwardingAudioStreamFactory::Core::RemoveOutput(
    AudioStreamBroker* broker) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  size_t removed = outputs_.erase(broker);
  CHECK_EQ(1u, removed, base::NotFatalUntil::M160);

  ResetRemoteFactoryPtrIfIdle();
}

media::mojom::AudioStreamFactory*
ForwardingAudioStreamFactory::Core::GetFactory() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  if (!remote_factory_) {
    TRACE_EVENT_INSTANT("audio",
                        "ForwardingAudioStreamFactory: Binding new factory",
                        "group", group_id_.GetLowForSerialization());
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindStreamFactoryFromUIThread,
                       remote_factory_.BindNewPipeAndPassReceiver()));
    // Unretained is safe because |this| owns |remote_factory_|.
    remote_factory_.set_disconnect_handler(base::BindOnce(
        &ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtr,
        base::Unretained(this)));

    // Restore the muting session on reconnect.
    if (muter_) {
      muter_->Connect(remote_factory_.get());
    }
  }

  return remote_factory_.get();
}

void ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtrIfIdle() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  if (inputs_.empty() && outputs_.empty()) {
    ResetRemoteFactoryPtr();
  }
}

void ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtr() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  if (remote_factory_) {
    TRACE_EVENT_INSTANT("audio",
                        "ForwardingAudioStreamFactory: Resetting factory",
                        "group", group_id_.GetLowForSerialization());
  }
  remote_factory_.reset();
  // The stream brokers will call a callback to be deleted soon, give them a
  // chance to signal an error to the client first.
}

}  // namespace content
