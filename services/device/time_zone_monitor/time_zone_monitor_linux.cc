// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

#if !defined(OS_CHROMEOS)

namespace device {

namespace {
class TimeZoneMonitorLinuxImpl;
}  // namespace

class TimeZoneMonitorLinux : public TimeZoneMonitor {
 public:
  TimeZoneMonitorLinux(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~TimeZoneMonitorLinux() override;

  void NotifyClientsFromImpl() { NotifyClients(); }

 private:
  scoped_refptr<TimeZoneMonitorLinuxImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorLinux);
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

  void StopWatching() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    owner_ = NULL;
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
        main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        file_task_runner_(file_task_runner),
        owner_(owner) {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  }

  ~TimeZoneMonitorLinuxImpl() { DCHECK(!owner_); }

  void StartWatchingOnFileThread() {
    base::AssertBlockingAllowedDeprecated();
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

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

    auto callback =
        base::BindRepeating(&TimeZoneMonitorLinuxImpl::OnTimeZoneFileChanged,
                            base::RetainedRef(this));
    for (size_t index = 0; index < arraysize(kFilesToWatch); ++index) {
      file_path_watchers_.push_back(std::make_unique<base::FilePathWatcher>());
      file_path_watchers_.back()->Watch(base::FilePath(kFilesToWatch[index]),
                                        false, callback);
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
  TimeZoneMonitorLinux* owner_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorLinuxImpl);
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
  return std::unique_ptr<TimeZoneMonitor>(
      new TimeZoneMonitorLinux(file_task_runner));
}

}  // namespace device

#endif  // !OS_CHROMEOS
