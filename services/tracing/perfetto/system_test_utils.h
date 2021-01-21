// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_
#define SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_

#include <string>

#include "services/tracing/public/cpp/perfetto/posix_system_producer.h"

namespace base {
class ScopedTempDir;
}

namespace perfetto {
class ServiceIPCHost;
class TracingService;
namespace base {
class TaskRunner;
}
}  // namespace perfetto

namespace tracing {

class MockSystemService {
 public:
  MockSystemService(const std::string& consumer_socket,
                    const std::string& producer_socket);
  MockSystemService(const base::ScopedTempDir& tmp_dir);
  ~MockSystemService();

  perfetto::TracingService* GetService();
  const std::string& consumer() const;
  const std::string& producer() const;

 private:
  void StartService();

  const bool used_tmpdir_;
  const char* old_tmpdir_ = nullptr;
  std::string consumer_;
  std::string producer_;
  std::unique_ptr<perfetto::ServiceIPCHost> service_;
  std::unique_ptr<perfetto::base::TaskRunner> task_runner_;
};

class MockPosixSystemProducer : public PosixSystemProducer {
 public:
  MockPosixSystemProducer(
      const std::string& socket,
      bool check_sdk_level = false,
      uint32_t num_data_sources = 0,
      base::OnceClosure data_source_enabled_callback = base::OnceClosure(),
      base::OnceClosure data_source_disabled_callback = base::OnceClosure());

  ~MockPosixSystemProducer() override;

  void StartDataSource(
      perfetto::DataSourceInstanceID id,
      const perfetto::DataSourceConfig& data_source_config) override;

  void StopDataSource(perfetto::DataSourceInstanceID id) override;

  void SetDataSourceEnabledCallback(
      base::OnceClosure data_source_enabled_callback);

  void SetDataSourceDisabledCallback(
      base::OnceClosure data_source_disabled_callback);

 private:
  uint32_t num_data_sources_expected_;
  uint32_t num_data_sources_active_ = 0;
  base::OnceClosure data_source_enabled_callback_;
  base::OnceClosure data_source_disabled_callback_;
  std::unique_ptr<SystemProducer> old_producer_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_
