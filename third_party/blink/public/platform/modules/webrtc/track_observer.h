// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_TRACK_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_TRACK_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace blink {

// TODO(crbug.com/704136): Move the classes below out of the Blink exposed
// API when all users of it have been Onion souped.
class BLINK_PLATFORM_EXPORT TrackObserver {
 public:
  TrackObserver(const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
                const scoped_refptr<webrtc::MediaStreamTrackInterface>& track);
  ~TrackObserver();

  using OnChangedCallback = base::RepeatingCallback<void(
      webrtc::MediaStreamTrackInterface::TrackState)>;
  void SetCallback(const OnChangedCallback& callback);

  const scoped_refptr<webrtc::MediaStreamTrackInterface>& track() const;

 private:
  class TrackObserverImpl;
  const scoped_refptr<TrackObserverImpl> observer_;
  DISALLOW_COPY_AND_ASSIGN(TrackObserver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_TRACK_OBSERVER_H_
