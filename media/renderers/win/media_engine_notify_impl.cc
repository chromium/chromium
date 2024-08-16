// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_engine_notify_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "media/base/win/mf_helpers.h"

namespace media {

namespace {

#define ENUM_TO_STRING(enum) \
  case enum:                 \
    return #enum

std::string MediaEngineErrorToString(MF_MEDIA_ENGINE_ERR error) {
  switch (error) {
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_NOERROR);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_ABORTED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_NETWORK);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_DECODE);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_ENCRYPTED);
    ENUM_TO_STRING(MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED);
    default:
      return "Unknown MF_MEDIA_ENGINE_ERR";
  }
}

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
    MF_MEDIA_ENGINE_ERR media_engine_error) {
  switch (media_engine_error) {
    case MF_MEDIA_ENGINE_ERR_NOERROR:
      return PIPELINE_OK;
    case MF_MEDIA_ENGINE_ERR_ABORTED:
      return PIPELINE_ERROR_ABORT;
    case MF_MEDIA_ENGINE_ERR_NETWORK:
      return PIPELINE_ERROR_NETWORK;
    case MF_MEDIA_ENGINE_ERR_DECODE:
      [[fallthrough]];
    case MF_MEDIA_ENGINE_ERR_ENCRYPTED:
      return PIPELINE_ERROR_DECODE;
    case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED:
      return DEMUXER_ERROR_COULD_NOT_OPEN;
    default:
      NOTREACHED();
  }
}

}  // namespace

MediaEngineNotifyImpl::MediaEngineNotifyImpl() = default;
MediaEngineNotifyImpl::~MediaEngineNotifyImpl() = default;

HRESULT MediaEngineNotifyImpl::RuntimeClassInitialize(
    ErrorCB error_cb,
    EndedCB ended_cb,
    FormatChangeCB format_change_cb,
    LoadedDataCB loaded_data_cb,
    CanPlayThroughCB can_play_through_cb,
    PlayingCB playing_cb,
    WaitingCB waiting_cb,
    FrameStepCompletedCB frame_step_completed_cb,
    TimeUpdateCB time_update_cb) {
  DVLOG_FUNC(1);

  error_cb_ = std::move(error_cb);
  ended_cb_ = std::move(ended_cb);
  format_change_cb_ = std::move(format_change_cb);
  loaded_data_cb_ = std::move(loaded_data_cb);
  can_play_through_cb_ = std::move(can_play_through_cb);
  playing_cb_ = std::move(playing_cb);
  waiting_cb_ = std::move(waiting_cb);
  frame_step_completed_cb_ = std::move(frame_step_completed_cb);
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

      // Report the HRESULT corresponding to certain MF_MEDIA_ENGINE_ERR
      // TODO(b/315860185): Remove this after the investigation is done.
      base::UmaHistogramSparse(
          base::StrCat({"Media.MediaFoundation.MediaEngineError.",
                        MediaEngineErrorToString(error), ".Hresult"}),
          hr);
      error_cb_.Run(MediaEngineErrorToPipelineStatus(error), hr);
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_ENDED:
      ended_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
      format_change_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
      loaded_data_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH:
      can_play_through_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      playing_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_WAITING:
      waiting_cb_.Run();
      break;
    case MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED:
      frame_step_completed_cb_.Run();
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
