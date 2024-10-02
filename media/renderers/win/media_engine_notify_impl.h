// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_ENGINE_NOTIFY_IMPL_H_
#define MEDIA_RENDERERS_WIN_MEDIA_ENGINE_NOTIFY_IMPL_H_

#include <mfmediaengine.h>
#include <wrl.h>

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"

namespace media {

// Implements IMFMediaEngineNotify required by IMFMediaEngine
// (https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengine).
//
class MediaEngineNotifyImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaEngineNotify> {
 public:
  MediaEngineNotifyImpl();
  ~MediaEngineNotifyImpl() override;

  using ErrorCB = base::RepeatingCallback<void(PipelineStatus, HRESULT)>;
  using EndedCB = base::RepeatingClosure;
  using FormatChangeCB = base::RepeatingClosure;
  using LoadedDataCB = base::RepeatingClosure;
  using CanPlayThroughCB = base::RepeatingClosure;
  using PlayingCB = base::RepeatingClosure;
  using WaitingCB = base::RepeatingClosure;
  using FrameStepCompletedCB = base::RepeatingClosure;
  using TimeUpdateCB = base::RepeatingClosure;

  HRESULT RuntimeClassInitialize(ErrorCB error_cb,
                                 EndedCB ended_cb,
                                 FormatChangeCB format_change_cb,
                                 LoadedDataCB loaded_data_cb,
                                 CanPlayThroughCB can_play_through_cb,
                                 PlayingCB playing_cb,
                                 WaitingCB waiting_cb,
                                 FrameStepCompletedCB frame_step_completed_cb,
                                 TimeUpdateCB time_update_cb);

  // IMFMediaEngineNotify implementation.
  IFACEMETHODIMP EventNotify(DWORD event_code,
                             DWORD_PTR param1,
                             DWORD param2) override;

  void Shutdown();

 private:
  // Callbacks are called on the MF threadpool thread and the creator of this
  // object must make sure the callbacks are safe to be called on that thread,
  // e.g. using base::BindPostTaskToCurrentDefault().
  ErrorCB error_cb_;
  EndedCB ended_cb_;
  FormatChangeCB format_change_cb_;
  LoadedDataCB loaded_data_cb_;
  CanPlayThroughCB can_play_through_cb_;
  PlayingCB playing_cb_;
  WaitingCB waiting_cb_;
  FrameStepCompletedCB frame_step_completed_cb_;
  TimeUpdateCB time_update_cb_;

  // EventNotify is invoked from MF threadpool thread where the callbacks are
  // called.
  // Shutdown is invoked from media stack thread. When this object is shutting
  // down, callbacks should not be called.
  base::Lock lock_;
  bool has_shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_ENGINE_NOTIFY_IMPL_H_
