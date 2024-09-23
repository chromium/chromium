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
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/audio_parameters.h"
#include "media/base/user_input_monitor.h"
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    media::UserInputMonitorBase* user_input_monitor,
    std::unique_ptr<AudioStreamBrokerFactory> broker_factory)
    : user_input_monitor_(user_input_monitor),
      owner_(std::move(owner)),
      broker_factory_(std::move(broker_factory)),
      group_id_(base::UnguessableToken::Create()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(owner_);
  DCHECK(broker_factory_);
}

ForwardingAudioStreamFactory::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (AudioStreamBroker::LoopbackSink* sink : loopback_sinks_)
    sink->OnSourceGone();
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |this| owns |inputs_|, so Unretained is safe.
  inputs_
      .insert(broker_factory_->CreateAudioInputStreamBroker(
          render_process_id, render_frame_id, device_id, params,
          shared_memory_count, user_input_monitor_, enable_agc,
          std::move(processing_config),
          base::BindOnce(&ForwardingAudioStreamFactory::Core::RemoveInput,
                         base::Unretained(this)),
          std::move(renderer_factory_client)))
      .first->get()
      ->CreateStream(GetFactory());
}

void ForwardingAudioStreamFactory::Core::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& raw_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
    const std::string& device_id,
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |this| owns |outputs_|, so Unretained is safe.
  outputs_
      .insert(broker_factory_->CreateAudioOutputStreamBroker(
          render_process_id, render_frame_id, ++stream_id_counter_, device_id,
          params, group_id_,
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(loopback_source);

  TRACE_EVENT_BEGIN1("audio", "CreateLoopbackStream", "group",
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
  TRACE_EVENT_END1("audio", "CreateLoopbackStream", "source",
                   loopback_source->GetGroupID().GetLowForSerialization());
}

void ForwardingAudioStreamFactory::Core::SetMuted(bool muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(muted, !!muter_);
  TRACE_EVENT_INSTANT2("audio", "SetMuted", TRACE_EVENT_SCOPE_THREAD, "group",
                       group_id_.GetLowForSerialization(), "muted", muted);

  if (!muted) {
    muter_.reset();
    return;
  }

  muter_.emplace(group_id_);
  if (remote_factory_)
    muter_->Connect(remote_factory_.get());
}

void ForwardingAudioStreamFactory::Core::AddLoopbackSink(
    AudioStreamBroker::LoopbackSink* sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  loopback_sinks_.insert(sink);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ForwardingAudioStreamFactory::LoopbackStreamStarted,
                     owner_));
}

void ForwardingAudioStreamFactory::Core::RemoveLoopbackSink(
    AudioStreamBroker::LoopbackSink* sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  loopback_sinks_.erase(sink);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ForwardingAudioStreamFactory::LoopbackStreamStopped,
                     owner_));
}

const base::UnguessableToken& ForwardingAudioStreamFactory::Core::GetGroupID() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return group_id();
}

// static
ForwardingAudioStreamFactory* ForwardingAudioStreamFactory::ForFrame(
    RenderFrameHost* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));
  if (!contents)
    return nullptr;

  return contents->GetAudioStreamFactory();
}

// static
ForwardingAudioStreamFactory::Core* ForwardingAudioStreamFactory::CoreForFrame(
    RenderFrameHost* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ForwardingAudioStreamFactory* forwarding_factory =
      ForwardingAudioStreamFactory::ForFrame(frame);
  return forwarding_factory ? forwarding_factory->core() : nullptr;
}

ForwardingAudioStreamFactory::ForwardingAudioStreamFactory(
    WebContents* web_contents,
    media::UserInputMonitorBase* user_input_monitor,
    std::unique_ptr<AudioStreamBrokerFactory> broker_factory)
    : WebContentsObserver(web_contents), core_() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_ = std::make_unique<Core>(weak_ptr_factory_.GetWeakPtr(),
                                 user_input_monitor, std::move(broker_factory));
}

ForwardingAudioStreamFactory::~ForwardingAudioStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(core_)));
}

void ForwardingAudioStreamFactory::LoopbackStreamStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  capture_handle_ = web_contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
}

void ForwardingAudioStreamFactory::LoopbackStreamStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  capture_handle_.RunAndReset();
}

void ForwardingAudioStreamFactory::SetMuted(bool muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_muted_;
}

void ForwardingAudioStreamFactory::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);

  // Unretained is safe since the destruction of |core_| will be posted to the
  // IO thread later.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::CleanupStreamsBelongingTo,
                                base::Unretained(core_.get()),
                                render_frame_host->GetProcess()->GetID(),
                                render_frame_host->GetRoutingID()));
}

void ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
    AudioStreamFactoryBinder binder) {
  GetAudioStreamFactoryBinderOverride() = std::move(binder);
}

void ForwardingAudioStreamFactory::Core::CleanupStreamsBelongingTo(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  TRACE_EVENT_BEGIN2("audio", "CleanupStreamsBelongingTo", "group",
                     group_id_.GetLowForSerialization(), "process id",
                     render_process_id);

  auto match_rfh =
      [render_process_id, render_frame_id](
          const std::unique_ptr<AudioStreamBroker>& broker) -> bool {
    return broker->render_process_id() == render_process_id &&
           broker->render_frame_id() == render_frame_id;
  };

  base::EraseIf(outputs_, match_rfh);
  base::EraseIf(inputs_, match_rfh);

  ResetRemoteFactoryPtrIfIdle();

  TRACE_EVENT_END1("audio", "CleanupStreamsBelongingTo", "frame_id",
                   render_frame_id);
}

void ForwardingAudioStreamFactory::Core::RemoveInput(
    AudioStreamBroker* broker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  size_t removed = inputs_.erase(broker);
  DCHECK_EQ(1u, removed);

  ResetRemoteFactoryPtrIfIdle();
}

void ForwardingAudioStreamFactory::Core::RemoveOutput(
    AudioStreamBroker* broker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  size_t removed = outputs_.erase(broker);
  DCHECK_EQ(1u, removed);

  ResetRemoteFactoryPtrIfIdle();
}

media::mojom::AudioStreamFactory*
ForwardingAudioStreamFactory::Core::GetFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!remote_factory_) {
    TRACE_EVENT_INSTANT1(
        "audio", "ForwardingAudioStreamFactory: Binding new factory",
        TRACE_EVENT_SCOPE_THREAD, "group", group_id_.GetLowForSerialization());
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindStreamFactoryFromUIThread,
                       remote_factory_.BindNewPipeAndPassReceiver()));
    // Unretained is safe because |this| owns |remote_factory_|.
    remote_factory_.set_disconnect_handler(base::BindOnce(
        &ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtr,
        base::Unretained(this)));

    // Restore the muting session on reconnect.
    if (muter_)
      muter_->Connect(remote_factory_.get());
  }

  return remote_factory_.get();
}

void ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtrIfIdle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (inputs_.empty() && outputs_.empty())
    ResetRemoteFactoryPtr();
}

void ForwardingAudioStreamFactory::Core::ResetRemoteFactoryPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (remote_factory_) {
    TRACE_EVENT_INSTANT1(
        "audio", "ForwardingAudioStreamFactory: Resetting factory",
        TRACE_EVENT_SCOPE_THREAD, "group", group_id_.GetLowForSerialization());
  }
  remote_factory_.reset();
  // The stream brokers will call a callback to be deleted soon, give them a
  // chance to signal an error to the client first.
}

}  // namespace content
