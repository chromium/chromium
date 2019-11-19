// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/certificate_watcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

namespace {

// Delay before re-reading the cert DB when a change is detected.
const int kReadDelayInSeconds = 2;

// Full Path: $HOME/.pki/nssdb
const char kCertDirectoryPath[] = ".pki/nssdb";

const char* const kCertFiles[] = {"cert9.db", "key4.db", "pkcs11.txt"};

}  // namespace

// This class lives on the IO thread, watches the certificate database files,
// and notifies the caller_task_runner thread of any updates.
class CertDbContentWatcher {
 public:
  CertDbContentWatcher(
      base::WeakPtr<CertificateWatcher> watcher,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      base::FilePath cert_watch_path,
      base::TimeDelta read_delay);
  ~CertDbContentWatcher();

  void StartWatching();

 private:
  base::ThreadChecker thread_checker_;

  // size_t is the return type of base::HashInts() which is used to accumulate
  // a hash-code while iterating over the database files.
  typedef size_t HashValue;

  // Called by the FileWatcher when it detects any changes.
  void OnCertDirectoryChanged(const base::FilePath& path, bool error);

  // Called by |read_timer_| to trigger re-reading the DB content.
  void OnTimer();

  // Reads the certificate database files and returns a hash of their contents.
  HashValue ComputeHash();

  base::WeakPtr<CertificateWatcher> watcher_;

  // The TaskRunner to be notified of any changes.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // The file watcher to watch changes inside the certificate folder.
  std::unique_ptr<base::FilePathWatcher> file_watcher_;

  // Path of the certificate files/directories.
  base::FilePath cert_watch_path_;

  // Timer to delay reading the DB files after a change notification from the
  // FileWatcher. This is done to avoid triggering multiple notifications when
  // the DB is written to. It also avoids a false notification in case the NSS
  // DB content is quickly changed and reverted.
  std::unique_ptr<base::DelayTimer> read_timer_;

  // The time to wait before re-reading the DB files after a change is
  // detected.
  base::TimeDelta delay_;

  // The hash code of the current certificate database contents. When the
  // FileWatcher detects changes, the code is re-computed and compared with
  // this stored value.
  HashValue current_hash_;

  DISALLOW_COPY_AND_ASSIGN(CertDbContentWatcher);
};

CertDbContentWatcher::CertDbContentWatcher(
    base::WeakPtr<CertificateWatcher> watcher,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::FilePath cert_watch_path,
    base::TimeDelta read_delay)
    : watcher_(watcher),
      caller_task_runner_(caller_task_runner),
      cert_watch_path_(cert_watch_path),
      delay_(read_delay) {
  thread_checker_.DetachFromThread();
}

CertDbContentWatcher::~CertDbContentWatcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void CertDbContentWatcher::StartWatching() {
  DCHECK(!cert_watch_path_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  file_watcher_.reset(new base::FilePathWatcher());

  // Initialize hash value.
  current_hash_ = ComputeHash();

  // base::Unretained() is safe since this class owns the FileWatcher.
  file_watcher_->Watch(cert_watch_path_, true,
                       base::Bind(&CertDbContentWatcher::OnCertDirectoryChanged,
                                  base::Unretained(this)));

  read_timer_.reset(new base::DelayTimer(FROM_HERE, delay_, this,
                                         &CertDbContentWatcher::OnTimer));
}

void CertDbContentWatcher::OnCertDirectoryChanged(const base::FilePath& path,
                                                  bool error) {
  DCHECK(path == cert_watch_path_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error) {
    LOG(FATAL) << "Error occurred while watching for changes of file: "
               << cert_watch_path_.MaybeAsASCII();
  }

  read_timer_->Reset();
}

void CertDbContentWatcher::OnTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  HashValue new_hash = ComputeHash();
  if (new_hash != current_hash_) {
    current_hash_ = new_hash;
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CertificateWatcher::DatabaseChanged, watcher_));
  } else {
    VLOG(1) << "Directory changed but contents are the same.";
  }
}

CertDbContentWatcher::HashValue CertDbContentWatcher::ComputeHash() {
  DCHECK(thread_checker_.CalledOnValidThread());

  HashValue result = 0;

  for (const char* file : kCertFiles) {
    base::FilePath path = cert_watch_path_.AppendASCII(file);
    std::string content;
    HashValue file_hash = 0;

    // It's possible the file might not exist. Compute the overall hash in a
    // consistent way for the set of files that do exist. If a new file comes
    // into existence, the resulting hash-code should change.
    if (base::ReadFileToString(path, &content)) {
      file_hash = base::Hash(content);
    }
    result = base::HashInts(result, file_hash);
  }
  return result;
}

CertificateWatcher::CertificateWatcher(
    const base::Closure& restart_action,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : restart_action_(restart_action),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      delay_(base::TimeDelta::FromSeconds(kReadDelayInSeconds)) {
  if (!base::PathService::Get(base::DIR_HOME, &cert_watch_path_)) {
    LOG(FATAL) << "Failed to get path of the home directory.";
  }
  cert_watch_path_ = cert_watch_path_.AppendASCII(kCertDirectoryPath);
}

CertificateWatcher::~CertificateWatcher() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!is_started()) {
    return;
  }
  if (monitor_) {
    monitor_->RemoveStatusObserver(this);
  }
  io_task_runner_->DeleteSoon(FROM_HERE, content_watcher_.release());

  VLOG(1) << "Stopped watching certificate changes.";
}

void CertificateWatcher::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!cert_watch_path_.empty());

  content_watcher_.reset(new CertDbContentWatcher(weak_factory_.GetWeakPtr(),
                                                  caller_task_runner_,
                                                  cert_watch_path_, delay_));

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CertDbContentWatcher::StartWatching,
                                base::Unretained(content_watcher_.get())));

  VLOG(1) << "Started watching certificate changes.";
}

void CertificateWatcher::SetMonitor(scoped_refptr<HostStatusMonitor> monitor) {
  DCHECK(is_started());
  if (monitor_) {
    monitor_->RemoveStatusObserver(this);
  }
  monitor->AddStatusObserver(this);
  monitor_ = monitor;
}

void CertificateWatcher::OnClientConnected(const std::string& jid) {
  DCHECK(is_started());
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  inhibit_mode_ = true;
}

void CertificateWatcher::OnClientDisconnected(const std::string& jid) {
  DCHECK(is_started());
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  inhibit_mode_ = false;
  if (restart_pending_) {
    restart_pending_ = false;
    restart_action_.Run();
  }
}

void CertificateWatcher::SetDelayForTests(const base::TimeDelta& delay) {
  DCHECK(!is_started());
  delay_ = delay;
}

void CertificateWatcher::SetWatchPathForTests(
    const base::FilePath& watch_path) {
  DCHECK(!is_started());
  cert_watch_path_ = watch_path;
}

bool CertificateWatcher::is_started() const {
  return content_watcher_ != nullptr;
}

void CertificateWatcher::DatabaseChanged() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (inhibit_mode_) {
    restart_pending_ = true;
    return;
  }

  VLOG(1) << "Certificate was updated. Calling restart...";
  restart_action_.Run();
}

}  // namespace remoting
