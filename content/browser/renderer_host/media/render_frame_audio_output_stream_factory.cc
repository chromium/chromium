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
#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      TRACE_EVENT1("audio",
                   "RenderFrameAudioOutputStreamFactory::ProviderImpl::Acquire",
                   "raw device id", device_id_);

      base::WeakPtr<ForwardingAudioStreamFactory::Core> factory =
          owner_->forwarding_factory_;
      if (factory) {
        factory->CreateOutputStream(owner_->process_id_, owner_->frame_id_,
                                    device_id_, params,
                                    std::move(provider_client));
      }

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

  void DeleteProvider(media::mojom::AudioOutputStreamProvider* stream_provider);

  // Helper method for storing native logs.
  void SendLogMessage(const std::string& message) const;

  const int process_id_;
  const int frame_id_;
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RenderFrameAudioOutputStreamFactory::~RenderFrameAudioOutputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(core_)));
}

void RenderFrameAudioOutputStreamFactory::
    SetAuthorizedDeviceIdForGlobalMediaControls(std::string hashed_device_id) {
  core_->SetAuthorizedDeviceIdForGlobalMediaControls(
      std::move(hashed_device_id));
}

size_t
RenderFrameAudioOutputStreamFactory::CurrentNumberOfProvidersForTesting() {
  return core_->current_number_of_providers_for_testing();
}

RenderFrameAudioOutputStreamFactory::Core::Core(
    RenderFrameHost* frame,
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver)
    : process_id_(frame->GetProcess()->GetID()),
      frame_id_(frame->GetRoutingID()),
      authorization_handler_(audio_system, media_stream_manager, process_id_) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  receiver_.Bind(std::move(receiver));
}

void RenderFrameAudioOutputStreamFactory::Core::
    SetAuthorizedDeviceIdForGlobalMediaControls(std::string hashed_device_id) {
  authorization_handler_.SetAuthorizedDeviceIdForGlobalMediaControls(
      std::move(hashed_device_id));
}

void RenderFrameAudioOutputStreamFactory::Core::RequestDeviceAuthorization(
    mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider>
        provider_receiver,
    const std::optional<base::UnguessableToken>& session_id,
    const std::string& device_id,
    RequestDeviceAuthorizationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
      frame_id_, session_id.value_or(base::UnguessableToken()), device_id,
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

void RenderFrameAudioOutputStreamFactory::Core::DeleteProvider(
    media::mojom::AudioOutputStreamProvider* stream_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  size_t deleted = stream_providers_.erase(stream_provider);
  DCHECK_EQ(1u, deleted);
}

void RenderFrameAudioOutputStreamFactory::Core::SendLogMessage(
    const std::string& message) const {
  MediaStreamManager::SendMessageToNativeLog(
      "RFAOSF::" + message +
      base::StringPrintf(" [process_id=%d, frame_id=%d]", process_id_,
                         frame_id_));
}

}  // namespace content
