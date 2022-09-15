// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_STREAM_MONITOR_H_
#define SERVICES_AUDIO_STREAM_MONITOR_H_

#include "base/unguessable_token.h"

namespace audio {

class Snoopable;

class StreamMonitor {
 public:
  // Called when a stream in the group becomes active.
  virtual void OnStreamActive(Snoopable* snoopable) = 0;
  // Called when a stream in the group becomes inactive.
  virtual void OnStreamInactive(Snoopable* snoopable) = 0;

 protected:
  virtual ~StreamMonitor() = default;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_STREAM_MONITOR_H_
