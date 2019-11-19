// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/config_file_watcher.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

namespace remoting {

// The name of the command-line switch used to specify the host configuration
// file to use.
const char kHostConfigSwitchName[] = "host-config";

const base::FilePath::CharType kDefaultHostConfigFile[] =
    FILE_PATH_LITERAL("host.json");

#if defined(OS_WIN)
// Maximum number of times to try reading the configuration file before
// reporting an error.
const int kMaxRetries = 3;
#endif  // defined(OS_WIN)

class ConfigFileWatcherImpl
    : public base::RefCountedThreadSafe<ConfigFileWatcherImpl> {
 public:
  // Creates a configuration file watcher that lives on the |io_task_runner|
  // thread but posts config file updates on on |main_task_runner|.
  ConfigFileWatcherImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const base::FilePath& config_path);


  // Notify |delegate| of config changes.
  void Watch(ConfigWatcher::Delegate* delegate);

  // Stops watching the configuration file.
  void StopWatching();

 private:
  friend class base::RefCountedThreadSafe<ConfigFileWatcherImpl>;
  virtual ~ConfigFileWatcherImpl();

  void FinishStopping();

  void WatchOnIoThread();

  // Called every time the host configuration file is updated.
  void OnConfigUpdated(const base::FilePath& path, bool error);

  // Called to notify the delegate of updates/errors in the main thread.
  void NotifyUpdate(const std::string& config);
  void NotifyError();

  // Reads the configuration file and passes it to the delegate.
  void ReloadConfig();

  std::string config_;
  base::FilePath config_path_;

  std::unique_ptr<base::DelayTimer> config_updated_timer_;

  // Number of times an attempt to read the configuration file failed.
  int retries_;

  // Monitors the host configuration file.
  std::unique_ptr<base::FilePathWatcher> config_watcher_;

  ConfigWatcher::Delegate* delegate_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::WeakPtrFactory<ConfigFileWatcherImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConfigFileWatcherImpl);
};

ConfigFileWatcher::ConfigFileWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::FilePath& config_path)
    : impl_(new ConfigFileWatcherImpl(main_task_runner,
                                      io_task_runner, config_path)) {
}

ConfigFileWatcher::~ConfigFileWatcher() {
  impl_->StopWatching();
  impl_ = nullptr;
}

void ConfigFileWatcher::Watch(ConfigWatcher::Delegate* delegate) {
  impl_->Watch(delegate);
}

ConfigFileWatcherImpl::ConfigFileWatcherImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::FilePath& config_path)
    : config_path_(config_path),
      retries_(0),
      delegate_(nullptr),
      main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void ConfigFileWatcherImpl::Watch(ConfigWatcher::Delegate* delegate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!delegate_);

  delegate_ = delegate;

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConfigFileWatcherImpl::WatchOnIoThread, this));
}

void ConfigFileWatcherImpl::WatchOnIoThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(!config_updated_timer_);
  DCHECK(!config_watcher_);

  // Create the timer that will be used for delayed-reading the configuration
  // file.
  config_updated_timer_.reset(
      new base::DelayTimer(FROM_HERE, base::TimeDelta::FromSeconds(2), this,
                           &ConfigFileWatcherImpl::ReloadConfig));

  // Start watching the configuration file.
  config_watcher_.reset(new base::FilePathWatcher());
  if (!config_watcher_->Watch(
          config_path_, false,
          base::Bind(&ConfigFileWatcherImpl::OnConfigUpdated, this))) {
    PLOG(ERROR) << "Couldn't watch file '" << config_path_.value() << "'";
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConfigFileWatcherImpl::NotifyError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // Force reloading of the configuration file at least once.
  ReloadConfig();
}

void ConfigFileWatcherImpl::StopWatching() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  weak_factory_.InvalidateWeakPtrs();
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConfigFileWatcherImpl::FinishStopping, this));
}

ConfigFileWatcherImpl::~ConfigFileWatcherImpl() {
  DCHECK(!config_updated_timer_);
  DCHECK(!config_watcher_);
}

void ConfigFileWatcherImpl::FinishStopping() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  config_updated_timer_.reset();
  config_watcher_.reset();
}

void ConfigFileWatcherImpl::OnConfigUpdated(const base::FilePath& path,
                                            bool error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Call ReloadConfig() after a short delay, so that we will not try to read
  // the updated configuration file before it has been completely written.
  // If the writer moves the new configuration file into place atomically,
  // this delay may not be necessary.
  if (!error && config_path_ == path)
    config_updated_timer_->Reset();
}

void ConfigFileWatcherImpl::NotifyError() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  delegate_->OnConfigWatcherError();
}

void ConfigFileWatcherImpl::NotifyUpdate(const std::string& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  delegate_->OnConfigUpdated(config_);
}

void ConfigFileWatcherImpl::ReloadConfig() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  std::string config;
  if (!base::ReadFileToString(config_path_, &config)) {
#if defined(OS_WIN)
    // EACCESS may indicate a locking or sharing violation. Retry a few times
    // before reporting an error.
    if (errno == EACCES && retries_ < kMaxRetries) {
      PLOG(WARNING) << "Failed to read '" << config_path_.value() << "'";

      retries_ += 1;
      config_updated_timer_->Reset();
      return;
    }
#endif  // defined(OS_WIN)

    PLOG(ERROR) << "Failed to read '" << config_path_.value() << "'";

    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConfigFileWatcherImpl::NotifyError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  retries_ = 0;

  // Post an updated configuration only if it has actually changed.
  if (config_ != config) {
    config_ = config;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConfigFileWatcherImpl::NotifyUpdate,
                                  weak_factory_.GetWeakPtr(), config_));
  }
}

}  // namespace remoting
