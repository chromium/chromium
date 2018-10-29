// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/old_render_frame_audio_output_stream_factory.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/browser/renderer_host/media/audio_output_stream_observer_impl.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/services/mojo_audio_output_stream_provider.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

// static
std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                BrowserThread::DeleteOnIOThread>
RenderFrameAudioOutputStreamFactoryHandle::CreateFactory(
    RendererAudioOutputStreamFactoryContext* context,
    int render_frame_id,
    mojom::RendererAudioOutputStreamFactoryRequest request) {
  std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                  BrowserThread::DeleteOnIOThread>
      handle(new RenderFrameAudioOutputStreamFactoryHandle(context,
                                                           render_frame_id));
  // Unretained is safe since |*handle| must be posted to the IO thread prior to
  // deletion.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&RenderFrameAudioOutputStreamFactoryHandle::Init,
                     base::Unretained(handle.get()), std::move(request)));
  return handle;
}

RenderFrameAudioOutputStreamFactoryHandle::
    ~RenderFrameAudioOutputStreamFactoryHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

RenderFrameAudioOutputStreamFactoryHandle::
    RenderFrameAudioOutputStreamFactoryHandle(
        RendererAudioOutputStreamFactoryContext* context,
        int render_frame_id)
    : impl_(render_frame_id, context), binding_(&impl_) {}

void RenderFrameAudioOutputStreamFactoryHandle::Init(
    mojom::RendererAudioOutputStreamFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  binding_.Bind(std::move(request));
}

OldRenderFrameAudioOutputStreamFactory::OldRenderFrameAudioOutputStreamFactory(
    int render_frame_id,
    RendererAudioOutputStreamFactoryContext* context)
    : render_frame_id_(render_frame_id),
      context_(context),
      weak_ptr_factory_(this) {
  DCHECK(context_);
}

OldRenderFrameAudioOutputStreamFactory::
    ~OldRenderFrameAudioOutputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_EXACT_LINEAR("Media.Audio.OutputStreamsCanceledByBrowser",
                             stream_providers_.size(), 50);
  // Make sure to close all streams.
  stream_providers_.clear();
}

void OldRenderFrameAudioOutputStreamFactory::RequestDeviceAuthorization(
    media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
    int32_t session_id,
    const std::string& device_id,
    RequestDeviceAuthorizationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const base::TimeTicks auth_start_time = base::TimeTicks::Now();

  context_->RequestDeviceAuthorization(
      render_frame_id_, session_id, device_id,
      base::BindOnce(
          &OldRenderFrameAudioOutputStreamFactory::AuthorizationCompleted,
          weak_ptr_factory_.GetWeakPtr(), auth_start_time,
          std::move(stream_provider_request), std::move(callback)));
}

void OldRenderFrameAudioOutputStreamFactory::AuthorizationCompleted(
    base::TimeTicks auth_start_time,
    media::mojom::AudioOutputStreamProviderRequest request,
    RequestDeviceAuthorizationCallback callback,
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& raw_device_id,
    const std::string& device_id_for_renderer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioOutputAuthorizationHandler::UMALogDeviceAuthorizationTime(
      auth_start_time);

  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    std::move(callback).Run(media::OutputDeviceStatus(status),
                            media::AudioParameters::UnavailableDeviceParams(),
                            std::string());
    return;
  }

  int stream_id = next_stream_id_++;
  std::unique_ptr<media::mojom::AudioOutputStreamObserver> observer =
      std::make_unique<AudioOutputStreamObserverImpl>(
          context_->GetRenderProcessId(), render_frame_id_, stream_id);
  // Since |context_| outlives |this| and |this| outlives |stream_providers_|,
  // unretained is safe.
  stream_providers_.insert(
      std::make_unique<media::MojoAudioOutputStreamProvider>(
          std::move(request),
          base::BindOnce(
              &RendererAudioOutputStreamFactoryContext::CreateDelegate,
              base::Unretained(context_), raw_device_id, render_frame_id_,
              stream_id),
          base::BindOnce(&OldRenderFrameAudioOutputStreamFactory::RemoveStream,
                         base::Unretained(this)),
          std::move(observer)));

  std::move(callback).Run(media::OutputDeviceStatus(status), params,
                          device_id_for_renderer);
}

void OldRenderFrameAudioOutputStreamFactory::RemoveStream(
    media::mojom::AudioOutputStreamProvider* stream_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  stream_providers_.erase(stream_provider);
}

}  // namespace content
