// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <inttypes.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/output_device_info.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

namespace {

const char* OutputDeviceStatusToString(media::OutputDeviceStatus status) {
  switch (status) {
    case media::OUTPUT_DEVICE_STATUS_OK:
      return "OK";
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      return "ERROR_NOT_FOUND";
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      return "ERROR_NOT_AUTHORIZED";
    case media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      return "ERROR_TIMED_OUT";
    case media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL:
      return "ERROR_INTERNAL";
  }
}

void CancelPrerenderingForOutputStreamOnUIThread(
    GlobalRenderFrameHostId frame_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (auto* frame = RenderFrameHostImpl::FromID(frame_id)) {
    frame->CancelPrerendering(
        PrerenderCancellationReason(PrerenderFinalStatus::kAudioOutput));
  }
}

}  // namespace

class RenderFrameAudioOutputStreamFactory::Core final
    : public blink::mojom::RendererAudioOutputStreamFactory {
 public:
  Core(RenderFrameHost* frame,
       media::AudioSystem* audio_system,
       MediaStreamManager* media_stream_manager,
       mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
           receiver);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() final = default;

  void Init(
      mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
          receiver);

  void SetAuthorizedDeviceIdForGlobalMediaControls(
      std::string hashed_device_id);

  void ReleaseRestriction();

  size_t current_number_of_providers_for_testing() {
    return stream_providers_.size();
  }

 private:
  // This class implements media::mojom::AudioOutputStreamProvider for a single
  // streams and cleans itself up (using the |owner| pointer) when done.
  class ProviderImpl final : public media::mojom::AudioOutputStreamProvider {
   public:
    ProviderImpl(
        mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider> receiver,
        RenderFrameAudioOutputStreamFactory::Core* owner,
        const std::string& device_id)
        : owner_(owner),
          device_id_(device_id),
          receiver_(this, std::move(receiver)) {
      CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
      // Unretained is safe since |this| owns |receiver_|.
      receiver_.set_disconnect_handler(
          base::BindOnce(&ProviderImpl::Done, base::Unretained(this)));
    }

    ProviderImpl(const ProviderImpl&) = delete;
    ProviderImpl& operator=(const ProviderImpl&) = delete;

    ~ProviderImpl() final { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

    void Acquire(
        const media::AudioParameters& params,
        mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
            provider_client) final {
      CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
      TRACE_EVENT1("audio",
                   "RenderFrameAudioOutputStreamFactory::ProviderImpl::Acquire",
                   "raw device id", device_id_);

      owner_->CreateOutputStream(device_id_, params,
                                 std::move(provider_client));

      // Since the stream creation has been propagated, |this| is no longer
      // needed.
      Done();
    }

    void Done() { owner_->DeleteProvider(this); }

   private:
    const raw_ptr<RenderFrameAudioOutputStreamFactory::Core> owner_;
    const std::string device_id_;

    mojo::Receiver<media::mojom::AudioOutputStreamProvider> receiver_;
  };

  using OutputStreamProviderSet =
      base::flat_set<std::unique_ptr<media::mojom::AudioOutputStreamProvider>,
                     base::UniquePtrComparator>;

  // blink::mojom::RendererAudioOutputStreamFactory implementation.
  void RequestDeviceAuthorization(
      mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider>
          provider_receiver,
      const std::optional<base::UnguessableToken>& session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback) final;

  // Here, the |raw_device_id| is used to create the stream, and
  // |device_id_for_renderer| is nonempty in the case when the renderer
  // requested a device using a |session_id|, to let it know which device was
  // chosen. This id is hashed.
  void AuthorizationCompleted(
      base::TimeTicks auth_start_time,
      mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider> receiver,
      RequestDeviceAuthorizationCallback callback,
      media::OutputDeviceStatus status,
      const media::AudioParameters& params,
      const std::string& raw_device_id,
      const std::string& device_id_for_renderer);

  // Forwards output stream creation to `forwarding_factory_`, or defers it via
  // `DeferOutputStreamUntilActivation()` while `is_prerendering_` is true.
  void CreateOutputStream(
      const std::string& device_id,
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
          provider_client);

  // Called by `CreateOutputStream()` while `is_prerendering_` is true. Queues
  // the stream request in `pending_streams_` and, for the first queued
  // request, asks the UI thread to cancel prerendering. If prerendering is
  // cancelled, `this` is destroyed together with the frame and the queued
  // requests are dropped. If it can't be cancelled (e.g. activation has
  // already started), the queued requests are created by
  // `ReleaseRestriction()` once the page is activated, so the renderer never
  // ends up waiting for a stream that will not be created.
  void DeferOutputStreamUntilActivation(
      const std::string& device_id,
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
          provider_client);

  void DeleteProvider(media::mojom::AudioOutputStreamProvider* stream_provider);

  // Helper method for storing native logs.
  void SendLogMessage(const std::string& message) const;

  const int process_id_;
  const GlobalRenderFrameHostId global_render_frame_host_id_;
  const GlobalRenderFrameHostToken main_frame_token_;
  // True if the frame's FrameTree was prerendering when `this` was constructed
  // on the UI thread and `ReleaseRestriction()` has not yet been called.
  // Accessed on the IO thread after construction.
  bool is_prerendering_;

  // Stream requests received while `is_prerendering_` is true. Created by
  // `ReleaseRestriction()`, or dropped if `this` is destroyed first.
  struct PendingStream {
    PendingStream(
        const std::string& device_id,
        const media::AudioParameters& params,
        mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
            provider_client);
    PendingStream(PendingStream&&);
    PendingStream& operator=(PendingStream&&);
    ~PendingStream();

    std::string device_id;
    media::AudioParameters params;
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        provider_client;
  };
  std::vector<PendingStream> pending_streams_;

  AudioOutputAuthorizationHandler authorization_handler_;

  mojo::Receiver<blink::mojom::RendererAudioOutputStreamFactory> receiver_{
      this};
  // Always null-check this weak pointer before dereferencing it.
  base::WeakPtr<ForwardingAudioStreamFactory::Core> forwarding_factory_;

  // The OutputStreamProviders for authorized streams are kept here while
  // waiting for the renderer to finish creating the stream, and destructed
  // afterwards.
  OutputStreamProviderSet stream_providers_;

  // Weak pointers are used to cancel device authorizations that are in flight
  // while |this| is destructed.
  base::WeakPtrFactory<Core> weak_ptr_factory_{this};
};

RenderFrameAudioOutputStreamFactory::RenderFrameAudioOutputStreamFactory(
    RenderFrameHost* frame,
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver)
    : core_(new Core(frame,
                     audio_system,
                     media_stream_manager,
                     std::move(receiver))) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M152);
}

RenderFrameAudioOutputStreamFactory::~RenderFrameAudioOutputStreamFactory() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M152);

  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(core_)));
}

void RenderFrameAudioOutputStreamFactory::
    SetAuthorizedDeviceIdForGlobalMediaControls(std::string hashed_device_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M152);
  // base::Unretained(core_.get()) is safe here because |core_| is owned by
  // RenderFrameAudioOutputStreamFactory and the
  // RenderFrameAudioOutputStreamFactory destructor posts a task to delete
  // |core_| on the IO thread. Since both tasks are posted to IO thread, the
  // task posted here will always execute before the destruction task.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SetAuthorizedDeviceIdForGlobalMediaControls,
                     base::Unretained(core_.get()),
                     std::move(hashed_device_id)));
}

void RenderFrameAudioOutputStreamFactory::ReleaseRestriction() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // base::Unretained(core_.get()) is safe here because |core_| is owned by
  // RenderFrameAudioOutputStreamFactory and the
  // RenderFrameAudioOutputStreamFactory destructor posts a task to delete
  // |core_| on the IO thread. Since both tasks are posted to IO thread, the
  // task posted here will always execute before the destruction task.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::ReleaseRestriction, base::Unretained(core_.get())));
}

size_t
RenderFrameAudioOutputStreamFactory::CurrentNumberOfProvidersForTesting() {
  return core_->current_number_of_providers_for_testing();
}

RenderFrameAudioOutputStreamFactory::Core::PendingStream::PendingStream(
    const std::string& device_id,
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        provider_client)
    : device_id(device_id),
      params(params),
      provider_client(std::move(provider_client)) {}

RenderFrameAudioOutputStreamFactory::Core::PendingStream::PendingStream(
    PendingStream&&) = default;
RenderFrameAudioOutputStreamFactory::Core::PendingStream&
RenderFrameAudioOutputStreamFactory::Core::PendingStream::operator=(
    PendingStream&&) = default;
RenderFrameAudioOutputStreamFactory::Core::PendingStream::~PendingStream() =
    default;

RenderFrameAudioOutputStreamFactory::Core::Core(
    RenderFrameHost* frame,
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver)
    : process_id_(frame->GetProcess()->GetDeprecatedID()),
      global_render_frame_host_id_(frame->GetGlobalId()),
      main_frame_token_(frame->GetMainFrame()->GetGlobalFrameToken()),
      // Like `RenderFrameHostImpl`'s `mojo_binder_policy_applier_`, use the
      // frame tree's prerendering state rather than the frame's lifecycle
      // state, so that all documents in a prerendering page are covered, even
      // ones not in `kPrerendering` state (e.g. while running unload handlers).
      is_prerendering_(base::FeatureList::IsEnabled(
                           kDisallowAudioOutputDuringPrerendering) &&
                       static_cast<RenderFrameHostImpl*>(frame)
                           ->frame_tree()
                           ->is_prerendering()),
      authorization_handler_(audio_system,
                             media_stream_manager,
                             global_render_frame_host_id_) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M152);

  ForwardingAudioStreamFactory::Core* tmp_factory =
      ForwardingAudioStreamFactory::CoreForFrame(frame);

  if (!tmp_factory) {
    // The only case when we not have a forwarding factory at this point is when
    // the frame belongs to an interstitial. Interstitials don't need audio, so
    // it's fine to drop the request.
    return;
  }

  forwarding_factory_ = tmp_factory->AsWeakPtr();

  // Unretained is safe since the destruction of |this| is posted to the IO
  // thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Init, base::Unretained(this), std::move(receiver)));
}

void RenderFrameAudioOutputStreamFactory::Core::Init(
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);

  receiver_.Bind(std::move(receiver));
}

void RenderFrameAudioOutputStreamFactory::Core::
    SetAuthorizedDeviceIdForGlobalMediaControls(std::string hashed_device_id) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
  authorization_handler_.SetAuthorizedDeviceIdForGlobalMediaControls(
      std::move(hashed_device_id));
}

void RenderFrameAudioOutputStreamFactory::Core::ReleaseRestriction() {
  CHECK_CURRENTLY_ON(BrowserThread::IO);
  is_prerendering_ = false;
  // Create streams requested before this notification arrived, e.g. because
  // prerendering couldn't be cancelled or the renderer's first `Acquire()`
  // after activation raced with this task.
  for (PendingStream& pending_stream : std::exchange(pending_streams_, {})) {
    CreateOutputStream(pending_stream.device_id, pending_stream.params,
                       std::move(pending_stream.provider_client));
  }
}

void RenderFrameAudioOutputStreamFactory::Core::RequestDeviceAuthorization(
    mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider>
        provider_receiver,
    const std::optional<base::UnguessableToken>& session_id,
    const std::string& device_id,
    RequestDeviceAuthorizationCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
  TRACE_EVENT2(
      "audio",
      "RenderFrameAudioOutputStreamFactory::RequestDeviceAuthorization",
      "device id", device_id, "session_id",
      session_id.value_or(base::UnguessableToken()).ToString());
  SendLogMessage(
      base::StringPrintf("%s({device_id=%s})", __func__, device_id.c_str()));

  const base::TimeTicks auth_start_time = base::TimeTicks::Now();

  AudioOutputAuthorizationHandler::AuthorizationCompletedCallback
      completed_callback = base::BindOnce(
          &RenderFrameAudioOutputStreamFactory::Core::AuthorizationCompleted,
          weak_ptr_factory_.GetWeakPtr(), auth_start_time,
          std::move(provider_receiver), std::move(callback));

  authorization_handler_.RequestDeviceAuthorization(
      session_id.value_or(base::UnguessableToken()), device_id,
      std::move(completed_callback));
}

void RenderFrameAudioOutputStreamFactory::Core::AuthorizationCompleted(
    base::TimeTicks auth_start_time,
    mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider> receiver,
    RequestDeviceAuthorizationCallback callback,
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& raw_device_id,
    const std::string& device_id_for_renderer) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
  TRACE_EVENT2("audio",
               "RenderFrameAudioOutputStreamFactory::AuthorizationCompleted",
               "raw device id", raw_device_id, "status", status);
  SendLogMessage(base::StringPrintf(
      "%s({status=%s}, {params=%s}, {device_id=%s})", __func__,
      OutputDeviceStatusToString(status),
      params.AsHumanReadableString().c_str(), raw_device_id.c_str()));
  SendLogMessage(base::StringPrintf(
      "%s => (authorization time=%" PRId64 " ms)", __func__,
      (base::TimeTicks::Now() - auth_start_time).InMilliseconds()));

  AudioOutputAuthorizationHandler::UMALogDeviceAuthorizationTime(
      auth_start_time);

  // If |status| is not OK, this call will be considered as an error signal by
  // the renderer.
  std::move(callback).Run(status, params, device_id_for_renderer);

  if (status == media::OUTPUT_DEVICE_STATUS_OK) {
    stream_providers_.insert(std::make_unique<ProviderImpl>(
        std::move(receiver), this, std::move(raw_device_id)));
  } else {
    SendLogMessage(base::StringPrintf("%s => (ERROR: %s)", __func__,
                                      OutputDeviceStatusToString(status)));
  }
}

void RenderFrameAudioOutputStreamFactory::Core::CreateOutputStream(
    const std::string& device_id,
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        provider_client) {
  CHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_prerendering_) {
    DeferOutputStreamUntilActivation(device_id, params,
                                     std::move(provider_client));
    return;
  }
  if (forwarding_factory_) {
    forwarding_factory_->CreateOutputStream(
        process_id_, global_render_frame_host_id_.frame_routing_id,
        main_frame_token_, device_id, params, std::move(provider_client));
  }
}

void RenderFrameAudioOutputStreamFactory::Core::
    DeferOutputStreamUntilActivation(
        const std::string& device_id,
        const media::AudioParameters& params,
        mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
            provider_client) {
  CHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(is_prerendering_);
  SendLogMessage(base::StringPrintf(
      "%s => (stream acquisition deferred until prerender activation)",
      __func__));
  if (pending_streams_.empty()) {
    // Only ask for cancellation once; repeated requests would be no-ops.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CancelPrerenderingForOutputStreamOnUIThread,
                                  global_render_frame_host_id_));
  }
  pending_streams_.emplace_back(device_id, params, std::move(provider_client));
}

void RenderFrameAudioOutputStreamFactory::Core::DeleteProvider(
    media::mojom::AudioOutputStreamProvider* stream_provider) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M152);
  size_t deleted = stream_providers_.erase(stream_provider);
  CHECK_EQ(1u, deleted, base::NotFatalUntil::M152);
}

void RenderFrameAudioOutputStreamFactory::Core::SendLogMessage(
    const std::string& message) const {
  MediaStreamManager::SendMessageToNativeLog(
      "RFAOSF::" + message +
      base::StringPrintf(" [process_id=%d, frame_id=%d]", process_id_,
                         global_render_frame_host_id_.frame_routing_id));
}

}  // namespace content
