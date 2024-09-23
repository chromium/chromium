// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

namespace {
class TimeZoneMonitorLinuxImpl;
}  // namespace

class TimeZoneMonitorLinux : public TimeZoneMonitor {
 public:
  TimeZoneMonitorLinux(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);

  TimeZoneMonitorLinux(const TimeZoneMonitorLinux&) = delete;
  TimeZoneMonitorLinux& operator=(const TimeZoneMonitorLinux&) = delete;

  ~TimeZoneMonitorLinux() override;

  void NotifyClientsFromImpl() {
#if BUILDFLAG(IS_CASTOS)
    // On CastOS, ICU's default time zone is already set to a new zone. No
    // need to redetect it with detectHostTimeZone() or to update ICU.
    // See http://b/112498903 and http://b/113344065.
    std::unique_ptr<icu::TimeZone> new_zone(icu::TimeZone::createDefault());
    NotifyClients(GetTimeZoneId(*new_zone));
#else
    std::unique_ptr<icu::TimeZone> new_zone(DetectHostTimeZoneFromIcu());

    // We get here multiple times on Linux per a single tz change, but
    // want to update the ICU default zone and notify renderer only once.
    // The timezone must have previously been populated. See InitializeICU().
    std::unique_ptr<icu::TimeZone> current_zone(icu::TimeZone::createDefault());
    if (*current_zone == *new_zone) {
      VLOG(1) << "timezone already updated";
      return;
    }

    UpdateIcuAndNotifyClients(std::move(new_zone));
#endif  // defined(IS_CASTOS)
  }

 private:
  scoped_refptr<TimeZoneMonitorLinuxImpl> impl_;
};

namespace {

// FilePathWatcher needs to run on the FILE thread, but TimeZoneMonitor runs
// on the UI thread. TimeZoneMonitorLinuxImpl is the bridge between these
// threads.
class TimeZoneMonitorLinuxImpl
    : public base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl> {
 public:
  static scoped_refptr<TimeZoneMonitorLinuxImpl> Create(
      TimeZoneMonitorLinux* owner,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
    auto impl = base::WrapRefCounted(
        new TimeZoneMonitorLinuxImpl(owner, file_task_runner));
    file_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&TimeZoneMonitorLinuxImpl::StartWatchingOnFileThread,
                       impl));
    return impl;
  }

  TimeZoneMonitorLinuxImpl(const TimeZoneMonitorLinuxImpl&) = delete;
  TimeZoneMonitorLinuxImpl& operator=(const TimeZoneMonitorLinuxImpl&) = delete;

  void StopWatching() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    owner_ = nullptr;
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TimeZoneMonitorLinuxImpl::StopWatchingOnFileThread,
                       base::RetainedRef(this)));
  }

 private:
  friend class base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl>;

  TimeZoneMonitorLinuxImpl(
      TimeZoneMonitorLinux* owner,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner)
      : base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl>(),
        main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        file_task_runner_(file_task_runner),
        owner_(owner) {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  }

  ~TimeZoneMonitorLinuxImpl() { DCHECK(!owner_); }

  void StartWatchingOnFileThread() {
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

    auto callback =
        base::BindRepeating(&TimeZoneMonitorLinuxImpl::OnTimeZoneFileChanged,
                            base::RetainedRef(this));

    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    // There is no true standard for where time zone information is actually
    // stored. glibc uses /etc/localtime, uClibc uses /etc/TZ, and some older
    // systems store the name of the time zone file within /usr/share/zoneinfo
    // in /etc/timezone. Different libraries and custom builds may mean that
    // still more paths are used. Just watch all three of these paths, because
    // false positives are harmless, assuming the false positive rate is
    // reasonable.
    const char* const kFilesToWatch[] = {
        "/etc/localtime", "/etc/timezone", "/etc/TZ",
    };
    for (size_t index = 0; index < std::size(kFilesToWatch); ++index) {
      file_path_watchers_.push_back(std::make_unique<base::FilePathWatcher>());
      file_path_watchers_.back()->Watch(
          base::FilePath(kFilesToWatch[index]),
          base::FilePathWatcher::Type::kNonRecursive, callback);
    }
  }

  void StopWatchingOnFileThread() {
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
    file_path_watchers_.clear();
  }

  void OnTimeZoneFileChanged(const base::FilePath& path, bool error) {
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TimeZoneMonitorLinuxImpl::OnTimeZoneFileChangedOnUIThread,
            base::RetainedRef(this)));
  }

  void OnTimeZoneFileChangedOnUIThread() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    if (owner_) {
      owner_->NotifyClientsFromImpl();
    }
  }

  std::vector<std::unique_ptr<base::FilePathWatcher>> file_path_watchers_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  raw_ptr<TimeZoneMonitorLinux> owner_;
};

}  // namespace

TimeZoneMonitorLinux::TimeZoneMonitorLinux(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : TimeZoneMonitor(), impl_() {
  // If the TZ environment variable is set, its value specifies the time zone
  // specification, and it's pointless to monitor any files in /etc for
  // changes because such changes would have no effect on the TZ environment
  // variable and thus the interpretation of the local time zone in the
  // or renderer processes.
  //
  // The system-specific format for the TZ environment variable beginning with
  // a colon is implemented by glibc as the path to a time zone data file, and
  // it would be possible to monitor this file for changes if a TZ variable of
  // this format was encountered, but this is not necessary: when loading a
  // time zone specification in this way, glibc does not reload the file when
  // it changes, so it's pointless to respond to a notification that it has
  // changed.
  if (!getenv("TZ")) {
    impl_ = TimeZoneMonitorLinuxImpl::Create(this, file_task_runner);
  }
}

TimeZoneMonitorLinux::~TimeZoneMonitorLinux() {
  if (impl_.get()) {
    impl_->StopWatching();
  }
}

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorLinux>(file_task_runner);
}

}  // namespace device
