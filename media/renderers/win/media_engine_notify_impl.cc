// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_engine_notify_impl.h"

#include "media/base/win/mf_helpers.h"

namespace media {

namespace {

#define ENUM_TO_STRING(enum) \
  case enum:                 \
    return #enum

std::string MediaEngineEventToString(MF_MEDIA_ENGINE_EVENT event) {
  switch (event) {
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_LOADSTART);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_PROGRESS);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_SUSPEND);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_ABORT);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_ERROR);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_EMPTIED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_STALLED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_PLAY);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_PAUSE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_LOADEDDATA);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_WAITING);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_PLAYING);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_CANPLAY);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_SEEKING);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_SEEKED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_TIMEUPDATE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_ENDED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_RATECHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_FORMATCHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_BALANCECHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_OPMINFO);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_RESOURCELOST);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE);
    default:
      return "Unknown MF_MEDIA_ENGINE_EVENT";
  }
}

#undef ENUM_TO_STRING

PipelineStatus MediaEngineErrorToPipelineStatus(
    MF_MEDIA_ENGINE_ERR media_engine_error,
    HRESULT hr) {
  // HRESULT 0x8004CD12 is DRM_E_TEE_INVALID_HWDRM_STATE, which can happen
  // during OS sleep/resume, or moving video to different graphics adapters.
  // This is not an error, so special case it here.
  if (hr == static_cast<HRESULT>(0x8004CD12))
    return PipelineStatus::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET;

  switch (media_engine_error) {
    case MF_MEDIA_ENGINE_ERR_NOERROR:
      return PipelineStatus::PIPELINE_OK;
    case MF_MEDIA_ENGINE_ERR_ABORTED:
      return PipelineStatus::PIPELINE_ERROR_ABORT;
    case MF_MEDIA_ENGINE_ERR_NETWORK:
      return PipelineStatus::PIPELINE_ERROR_NETWORK;
    case MF_MEDIA_ENGINE_ERR_DECODE:
      FALLTHROUGH;
    case MF_MEDIA_ENGINE_ERR_ENCRYPTED:
      return PipelineStatus::PIPELINE_ERROR_DECODE;
    case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED:
      return PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN;
    default:
      NOTREACHED();
      return PipelineStatus::PIPELINE_ERROR_INVALID_STATE;
  }
}

}  // namespace

MediaEngineNotifyImpl::MediaEngineNotifyImpl() = default;
MediaEngineNotifyImpl::~MediaEngineNotifyImpl() = default;

HRESULT MediaEngineNotifyImpl::RuntimeClassInitialize(
    ErrorCB error_cb,
    EndedCB ended_cb,
    BufferingStateChangedCB buffering_state_changed_cb,
    VideoNaturalSizeChangedCB video_natural_size_changed_cb,
    TimeUpdateCB time_update_cb) {
  DVLOG_FUNC(1);

  error_cb_ = std::move(error_cb);
  ended_cb_ = std::move(ended_cb);
  buffering_state_changed_cb_ = std::move(buffering_state_changed_cb);
  video_natural_size_changed_cb_ = std::move(video_natural_size_changed_cb);
  time_update_cb_ = std::move(time_update_cb);
  return S_OK;
}

// |param1| and |param2|'s meaning depends on the |event_code| from
// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/ne-mfmediaengine-mf_media_engine_event
// This method always return S_OK. Even for error |event_code| because we
// successfully handled the event.
HRESULT MediaEngineNotifyImpl::EventNotify(DWORD event_code,
                                           DWORD_PTR param1,
                                           DWORD param2) {
  auto event = static_cast<MF_MEDIA_ENGINE_EVENT>(event_code);
  DVLOG_FUNC(3) << "event=" << MediaEngineEventToString(event);

  base::AutoLock lock(lock_);
  if (has_shutdown_)
    return S_OK;

  switch (event) {
    case MF_MEDIA_ENGINE_EVENT_ERROR: {
      // |param1| - A member of the MF_MEDIA_ENGINE_ERR enumeration.
      // |param2| - An HRESULT error code, or zero.
      MF_MEDIA_ENGINE_ERR error = static_cast<MF_MEDIA_ENGINE_ERR>(param1);
      HRESULT hr = param2;
      LOG(ERROR) << __func__ << ": error=" << error << ", hr=" << PrintHr(hr);
      error_cb_.Run(MediaEngineErrorToPipelineStatus(error, hr), hr);
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_ENDED:
      ended_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
      video_natural_size_changed_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
      video_natural_size_changed_cb_.Run();
      FALLTHROUGH;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      buffering_state_changed_cb_.Run(
          BufferingState::BUFFERING_HAVE_ENOUGH,
          BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
      break;
    case MF_MEDIA_ENGINE_EVENT_WAITING:
      buffering_state_changed_cb_.Run(
          BufferingState::BUFFERING_HAVE_NOTHING,
          BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
      break;
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
      time_update_cb_.Run();
      break;

    default:
      DVLOG_FUNC(2) << "Unhandled event=" << MediaEngineEventToString(event);
      break;
  }
  return S_OK;
}

void MediaEngineNotifyImpl::Shutdown() {
  DVLOG_FUNC(1);

  base::AutoLock lock(lock_);
  has_shutdown_ = true;
}

}  // namespace media
