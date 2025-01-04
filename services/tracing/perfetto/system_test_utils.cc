// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/system_test_utils.h"

#include <cstdio>

#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "third_party/perfetto/protos/perfetto/common/commit_data_request.pb.h"

namespace tracing {

MockSystemService::MockSystemService(const std::string& consumer_socket,
                                     const std::string& producer_socket)
    : used_tmpdir_(false),
      consumer_(consumer_socket),
      producer_(producer_socket),
      task_runner_(std::make_unique<base::tracing::PerfettoTaskRunner>(
          base::SequencedTaskRunner::GetCurrentDefault())) {
  StartService();
}

MockSystemService::MockSystemService(const base::ScopedTempDir& tmp_dir)
    : MockSystemService(tmp_dir,
                        std::make_unique<base::tracing::PerfettoTaskRunner>(
                            base::SequencedTaskRunner::GetCurrentDefault())) {}

MockSystemService::MockSystemService(
    const base::ScopedTempDir& tmp_dir,
    std::unique_ptr<perfetto::base::TaskRunner> task_runner)
    : used_tmpdir_(true), task_runner_(std::move(task_runner)) {
  // We need to set TMPDIR environment variable because when a new producer
  // connects to the perfetto service it needs to create a memmap'd file for
  // the shared memory buffer. Setting TMPDIR allows the service to know
  // where this should be.
  //
  // Finally since environment variables are leaked into other tests if
  // multiple tests run we need to restore the value so each test is
  // hermetic.

  const auto* old_tmpdir = getenv("TMPDIR");
  if (old_tmpdir) {
    old_tmpdir_ = old_tmpdir;
  }
  setenv("TMPDIR", tmp_dir.GetPath().value().c_str(), true);
  // Set up the system socket locations in a valid tmp directory.
  producer_ = tmp_dir.GetPath().Append(FILE_PATH_LITERAL("producer")).value();
  consumer_ = tmp_dir.GetPath().Append(FILE_PATH_LITERAL("consumer")).value();
  StartService();
}

MockSystemService::~MockSystemService() {
  service_.reset();
  remove(producer().c_str());
  remove(consumer().c_str());
  if (used_tmpdir_) {
    if (old_tmpdir_) {
      // Restore the old value back to its initial value.
      setenv("TMPDIR", old_tmpdir_->c_str(), true);
    } else {
      // TMPDIR wasn't set originally so unset it.
      unsetenv("TMPDIR");
    }
  }
}

void MockSystemService::StartService() {
  service_ = perfetto::ServiceIPCHost::CreateInstance(task_runner_.get());
  CHECK(service_);
  unlink(producer_.c_str());
  unlink(consumer_.c_str());
  bool succeeded = service_->Start(producer_.c_str(), consumer_.c_str());
  CHECK(succeeded);
}

const std::string& MockSystemService::consumer() const {
  return consumer_;
}

const std::string& MockSystemService::producer() const {
  return producer_;
}

perfetto::TracingService* MockSystemService::GetService() {
  return service_->service();
}

}  // namespace tracing
