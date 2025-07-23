// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/profiling.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

std::string GetProfileName() {
  static base::NoDestructor<std::string> profile_name([]() {
    std::string profile_name;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kProfilingFile))
      profile_name = command_line.GetSwitchValueASCII(switches::kProfilingFile);
    else
      profile_name = std::string("chrome-profile-{type}-{pid}");
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);
    std::string type = process_type.empty() ? std::string("browser")
                                            : std::string(process_type);
    base::ReplaceSubstringsAfterOffset(&profile_name, 0, "{type}", type);

    return profile_name;
  }());

  return *profile_name;
}

void FlushProfilingData(base::Thread* thread) {
  static const int kProfilingFlushSeconds = 10;

  if (!Profiling::BeingProfiled())
    return;

  base::debug::FlushProfiling();
  static int flush_seconds;
  if (!flush_seconds) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string profiling_flush =
        command_line.GetSwitchValueASCII(switches::kProfilingFlush);
    if (!profiling_flush.empty()) {
      flush_seconds = atoi(profiling_flush.c_str());
      DCHECK(flush_seconds > 0);
    } else {
      flush_seconds = kProfilingFlushSeconds;
    }
  }
  thread->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&FlushProfilingData, thread),
      base::Seconds(flush_seconds));
}

class ProfilingThreadControl {
 public:
  ProfilingThreadControl() : thread_(nullptr) {}

  ProfilingThreadControl(const ProfilingThreadControl&) = delete;
  ProfilingThreadControl& operator=(const ProfilingThreadControl&) = delete;

  void Start() {
    base::AutoLock locked(lock_);

    if (thread_ && thread_->IsRunning())
      return;
    thread_ = new base::Thread("Profiling_Flush");
    thread_->Start();
    thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FlushProfilingData, thread_));
  }

  void Stop() {
    base::AutoLock locked(lock_);

    if (!thread_ || !thread_->IsRunning())
      return;
    thread_->Stop();
    delete thread_;
    thread_ = nullptr;
  }

 private:
  raw_ptr<base::Thread> thread_;
  base::Lock lock_;
};

base::LazyInstance<ProfilingThreadControl>::Leaky g_flush_thread_control =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void Profiling::ProcessStarted() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (command_line.HasSwitch(switches::kProfilingAtStart)) {
    std::string process_type_to_start =
        command_line.GetSwitchValueASCII(switches::kProfilingAtStart);
    if (process_type == process_type_to_start)
      Start();
  }
}

// static
void Profiling::Start() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  bool flush = command_line.HasSwitch(switches::kProfilingFlush);
  base::debug::StartProfiling(GetProfileName());

  // Schedule profile data flushing for single process because it doesn't
  // get written out correctly on exit.
  if (flush)
    g_flush_thread_control.Get().Start();
}

// static
void Profiling::Stop() {
  g_flush_thread_control.Get().Stop();
  base::debug::StopProfiling();
}

// static
bool Profiling::BeingProfiled() {
  return base::debug::BeingProfiled();
}

// static
void Profiling::Toggle() {
  if (BeingProfiled())
    Stop();
  else
    Start();
}

}  // namespace content
