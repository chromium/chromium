// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_REALTIME_AUDIO_THREAD_H_
#define SERVICES_AUDIO_REALTIME_AUDIO_THREAD_H_

#include "base/threading/thread.h"

namespace audio {

// Simple base::Thread which uses a configurable realtime thread period for Mac.
class RealtimeAudioThread : public base::Thread {
 public:
  RealtimeAudioThread(const std::string& name, base::TimeDelta realtime_period);
  ~RealtimeAudioThread() override;

#if BUILDFLAG(IS_APPLE)
  // base::PlatformThread::Delegate override.
  base::TimeDelta GetRealtimePeriod() override;
#endif

 private:
  base::TimeDelta realtime_period_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_REALTIME_AUDIO_THREAD_H_
