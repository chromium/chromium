// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/synchronous_host_resolver.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

namespace net {


namespace {

class ResolverThread : public base::SimpleThread {
 public:
  ResolverThread();

  ResolverThread(const ResolverThread&) = delete;
  ResolverThread& operator=(const ResolverThread&) = delete;

  ~ResolverThread() override;

  // Called on the main thread.
  int Resolve(url::SchemeHostPort scheme_host_port, AddressList* addresses);

  // SimpleThread methods:
  void Run() override;

 private:
  void OnResolutionComplete(base::OnceClosure on_done, int rv);

  AddressList* addresses_;
  url::SchemeHostPort scheme_host_port_;
  int rv_ = ERR_UNEXPECTED;
};

ResolverThread::ResolverThread() : SimpleThread("resolver_thread") {}

ResolverThread::~ResolverThread() = default;

void ResolverThread::Run() {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  net::HostResolver::ManagerOptions options;
  options.max_concurrent_resolves = 6;
  options.max_system_retry_attempts = 3u;
  std::unique_ptr<net::HostResolver> resolver =
      net::HostResolver::CreateStandaloneResolver(NetLog::Get(), options);

  // No need to use a NetworkAnonymizationKey here, since this is an external
  // tool not used by net/ consumers.
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(scheme_host_port_, NetworkAnonymizationKey(),
                              NetLogWithSource(), std::nullopt);

  base::RunLoop run_loop;
  rv_ = request->Start(base::BindOnce(&ResolverThread::OnResolutionComplete,
                                      base::Unretained(this),
                                      run_loop.QuitClosure()));

  if (rv_ == ERR_IO_PENDING) {
    // Run the message loop until OnResolutionComplete quits it.
    run_loop.Run();
  }

  if (rv_ == OK) {
    *addresses_ = *request->GetAddressResults();
  }
}

int ResolverThread::Resolve(url::SchemeHostPort scheme_host_port,
                            AddressList* addresses) {
  scheme_host_port_ = std::move(scheme_host_port);
  addresses_ = addresses;
  this->Start();
  this->Join();
  return rv_;
}

void ResolverThread::OnResolutionComplete(base::OnceClosure on_done, int rv) {
  rv_ = rv;
  std::move(on_done).Run();
}

}  // namespace

// static
int SynchronousHostResolver::Resolve(url::SchemeHostPort scheme_host_port,
                                     AddressList* addresses) {
  ResolverThread resolver;
  return resolver.Resolve(std::move(scheme_host_port), addresses);
}

}  // namespace net
