// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/net_log_exporter.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {

NetLogExporter::NetLogExporter(NetworkContext* network_context)
    : network_context_(network_context), state_(STATE_IDLE) {}

NetLogExporter::~NetLogExporter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // In case scratch directory creation didn't finish by the time |this| is
  // destroyed, |destination_| is still owned here (rather than handed over to
  // FileNetLogObserver); ask the scheduler to close it someplace suitable.
  if (destination_.IsValid())
    CloseFileOffThread(std::move(destination_));

  // ~FileNetLogObserver will take care of unregistering from NetLog even
  // if StopObserving isn't invoked.
}

void NetLogExporter::Start(base::File destination,
                           base::Value::Dict extra_constants,
                           net::NetLogCaptureMode capture_mode,
                           uint64_t max_file_size,
                           StartCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(destination.IsValid());

  if (state_ != STATE_IDLE) {
    CloseFileOffThread(std::move(destination));
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }

  // Store the file explicitly since destroying it involves disk I/O, so must
  // be carefully controlled.
  destination_ = std::move(destination);

  state_ = STATE_WAITING_DIR;
  static_assert(kUnlimitedFileSize == net::FileNetLogObserver::kNoLimit,
                "Inconsistent unbounded size constants");
  if (max_file_size != kUnlimitedFileSize) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NetLogExporter::CreateScratchDir,
                       scratch_dir_create_handler_for_tests_),

        // Note: this a static method which takes a weak pointer as an argument,
        // so it will run if |this| is deleted.
        base::BindOnce(&NetLogExporter::StartWithScratchDirOrCleanup,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(extra_constants), capture_mode, max_file_size,
                       std::move(callback)));
  } else {
    StartWithScratchDir(std::move(extra_constants), capture_mode, max_file_size,
                        std::move(callback), base::FilePath());
  }
}

void NetLogExporter::Stop(base::Value::Dict polled_data,
                          StopCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_RUNNING) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }

  base::Value::Dict net_info =
      net::GetNetInfo(network_context_->url_request_context());
  net_info.Merge(std::move(polled_data));

  file_net_observer_->StopObserving(
      std::make_unique<base::Value>(std::move(net_info)),
      base::BindOnce([](StopCallback sc) { std::move(sc).Run(net::OK); },
                     std::move(callback)));
  file_net_observer_ = nullptr;
  state_ = STATE_IDLE;
}

// static
base::FilePath NetLogExporter::CreateScratchDirForNetworkService(
    base::PassKey<NetworkService>) {
  return CreateScratchDir(base::RepeatingCallback<base::FilePath()>());
}

void NetLogExporter::SetCreateScratchDirHandlerForTesting(
    const base::RepeatingCallback<base::FilePath()>& handler) {
  scratch_dir_create_handler_for_tests_ = handler;
}

void NetLogExporter::CloseFileOffThread(base::File file) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (file.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce([](base::File f) { f.Close(); }, std::move(file)));
  }
}

base::FilePath NetLogExporter::CreateScratchDir(
    base::RepeatingCallback<base::FilePath()>
        scratch_dir_create_handler_for_tests) {
  if (scratch_dir_create_handler_for_tests)
    return scratch_dir_create_handler_for_tests.Run();

  base::ScopedTempDir scratch_dir;
  if (scratch_dir.CreateUniqueTempDir())
    return scratch_dir.Take();
  else
    return base::FilePath();
}

void NetLogExporter::StartWithScratchDirOrCleanup(
    base::WeakPtr<NetLogExporter> object,
    base::Value::Dict extra_constants,
    net::NetLogCaptureMode capture_mode,
    uint64_t max_file_size,
    StartCallback callback,
    const base::FilePath& scratch_dir_path) {
  NetLogExporter* instance = object.get();
  if (instance) {
    instance->StartWithScratchDir(std::move(extra_constants), capture_mode,
                                  max_file_size, std::move(callback),
                                  scratch_dir_path);
  } else if (!scratch_dir_path.empty()) {
    // An NetLogExporter got destroyed while it was trying to create a scratch
    // dir.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        // The delete is non-recursive since the only time this is invoked is
        // when the directory is expected to be empty.
        base::GetDeleteFileCallback(scratch_dir_path));
  }
}

void NetLogExporter::StartWithScratchDir(
    base::Value::Dict extra_constants,
    net::NetLogCaptureMode capture_mode,
    uint64_t max_file_size,
    StartCallback callback,
    const base::FilePath& scratch_dir_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (scratch_dir_path.empty() && max_file_size != kUnlimitedFileSize) {
    state_ = STATE_IDLE;
    CloseFileOffThread(std::move(destination_));
    std::move(callback).Run(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  state_ = STATE_RUNNING;

  base::Value::Dict constants = net::GetNetConstants();
  constants.Merge(std::move(extra_constants));

  if (max_file_size != kUnlimitedFileSize) {
    file_net_observer_ = net::FileNetLogObserver::CreateBoundedPreExisting(
        scratch_dir_path, std::move(destination_), max_file_size, capture_mode,
        std::make_unique<base::Value::Dict>(std::move(constants)));
  } else {
    DCHECK(scratch_dir_path.empty());
    file_net_observer_ = net::FileNetLogObserver::CreateUnboundedPreExisting(
        std::move(destination_), capture_mode,
        std::make_unique<base::Value::Dict>(std::move(constants)));
  }

  // There might not be a NetworkService object e.g. on iOS; in that case
  // assume this present NetworkContext is all there is.
  if (network_context_->network_service()) {
    network_context_->network_service()->CreateNetLogEntriesForActiveObjects(
        file_net_observer_.get());
  } else {
    std::set<net::URLRequestContext*> contexts;
    contexts.insert(network_context_->url_request_context());
    net::CreateNetLogEntriesForActiveObjects(contexts,
                                             file_net_observer_.get());
  }

  file_net_observer_->StartObserving(
      network_context_->url_request_context()->net_log());
  std::move(callback).Run(net::OK);
}

}  // namespace network
