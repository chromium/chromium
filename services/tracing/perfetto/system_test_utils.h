// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_
#define SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>

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
  explicit MockSystemService(const base::ScopedTempDir& tmp_dir);
  MockSystemService(const base::ScopedTempDir& tmp_dir,
                    std::unique_ptr<perfetto::base::TaskRunner>);
  ~MockSystemService();

  perfetto::TracingService* GetService();
  const std::string& consumer() const;
  const std::string& producer() const;

 private:
  void StartService();

  const bool used_tmpdir_;
  std::optional<std::string> old_tmpdir_;
  std::string consumer_;
  std::string producer_;
  std::unique_ptr<perfetto::ServiceIPCHost> service_;
  std::unique_ptr<perfetto::base::TaskRunner> task_runner_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_SYSTEM_TEST_UTILS_H_
