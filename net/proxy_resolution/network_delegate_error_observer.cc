// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/network_delegate_error_observer.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"

namespace net {

// NetworkDelegateErrorObserver::Core -----------------------------------------

class NetworkDelegateErrorObserver::Core
    : public base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core> {
 public:
  Core(NetworkDelegate* network_delegate,
       base::SingleThreadTaskRunner* origin_runner);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void NotifyPACScriptError(int line_number, const std::u16string& error);

  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core>;

  virtual ~Core();

  raw_ptr<NetworkDelegate> network_delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
};

NetworkDelegateErrorObserver::Core::Core(
    NetworkDelegate* network_delegate,
    base::SingleThreadTaskRunner* origin_runner)
    : network_delegate_(network_delegate), origin_runner_(origin_runner) {
  DCHECK(origin_runner);
}

NetworkDelegateErrorObserver::Core::~Core() = default;

void NetworkDelegateErrorObserver::Core::NotifyPACScriptError(
    int line_number,
    const std::u16string& error) {
  if (!origin_runner_->BelongsToCurrentThread()) {
    origin_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::NotifyPACScriptError, this, line_number, error));
    return;
  }
  if (network_delegate_)
    network_delegate_->NotifyPACScriptError(line_number, error);
}

void NetworkDelegateErrorObserver::Core::Shutdown() {
  CHECK(origin_runner_->BelongsToCurrentThread());
  network_delegate_ = nullptr;
}

// NetworkDelegateErrorObserver -----------------------------------------------

NetworkDelegateErrorObserver::NetworkDelegateErrorObserver(
    NetworkDelegate* network_delegate,
    base::SingleThreadTaskRunner* origin_runner)
    : core_(base::MakeRefCounted<Core>(network_delegate, origin_runner)) {}

NetworkDelegateErrorObserver::~NetworkDelegateErrorObserver() {
  core_->Shutdown();
}

// static
std::unique_ptr<ProxyResolverErrorObserver>
NetworkDelegateErrorObserver::Create(
    NetworkDelegate* network_delegate,
    const scoped_refptr<base::SingleThreadTaskRunner>& origin_runner) {
  return std::make_unique<NetworkDelegateErrorObserver>(network_delegate,
                                                        origin_runner.get());
}

void NetworkDelegateErrorObserver::OnPACScriptError(
    int line_number,
    const std::u16string& error) {
  core_->NotifyPACScriptError(line_number, error);
}

}  // namespace net
