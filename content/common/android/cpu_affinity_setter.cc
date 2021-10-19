// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_affinity_setter.h"

#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "base/timer/timer.h"

namespace content {

namespace {

class CpuAffinitySetter;

base::ThreadLocalOwnedPointer<CpuAffinitySetter>& GetCpuAffinitySetter() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<CpuAffinitySetter>>
      setter;
  return *setter;
}

class CpuAffinitySetter {
 public:
  CpuAffinitySetter() = default;
  ~CpuAffinitySetter() = default;

  void SetCpuAffinity(base::CpuAffinityMode mode) {
    mode_ = mode;
    SetModeInternal();

    if (mode == base::CpuAffinityMode::kDefault) {
      timer_.Stop();
    } else if (!timer_.IsRunning()) {
      timer_.Start(FROM_HERE, base::Seconds(15), this,
                   &CpuAffinitySetter::SetModeInternal);
    }
  }

 private:
  void SetModeInternal() {
    auto current = base::CurrentThreadCpuAffinityMode();
    if (!current || *current != mode_)
      base::SetThreadCpuAffinityMode(base::PlatformThread::CurrentId(), mode_);
  }

  base::CpuAffinityMode mode_;
  base::RepeatingTimer timer_;
};

}  // namespace

void SetCpuAffinityForCurrentThread(base::CpuAffinityMode mode) {
  auto& setter = GetCpuAffinitySetter();
  if (!setter.Get())
    setter.Set(std::make_unique<CpuAffinitySetter>());

  setter.Get()->SetCpuAffinity(mode);
}

}  // namespace content
