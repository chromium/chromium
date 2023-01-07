// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remoting_service.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/policy_watcher.h"

namespace remoting {

namespace {

class RemotingServiceImpl : public RemotingService {
 public:
  RemotingServiceImpl();
  RemotingServiceImpl(const RemotingServiceImpl&) = delete;
  RemotingServiceImpl& operator=(const RemotingServiceImpl&) = delete;
  ~RemotingServiceImpl() override;

  // RemotingService implementation.
  RemoteSupportHostAsh& GetSupportHost() override;
  std::unique_ptr<ChromotingHostContext> CreateHostContext() override;
  std::unique_ptr<PolicyWatcher> CreatePolicyWatcher() override;

 private:
  void ReleaseSupportHost();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RemoteSupportHostAsh> remote_support_host_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

RemotingServiceImpl::RemotingServiceImpl() = default;
RemotingServiceImpl::~RemotingServiceImpl() = default;

RemoteSupportHostAsh& RemotingServiceImpl::GetSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_support_host_) {
    remote_support_host_ =
        std::make_unique<RemoteSupportHostAsh>(base::BindOnce(
            &RemotingServiceImpl::ReleaseSupportHost, base::Unretained(this)));
  }
  return *remote_support_host_;
}

std::unique_ptr<ChromotingHostContext>
RemotingServiceImpl::CreateHostContext() {
  return ChromotingHostContext::CreateForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

std::unique_ptr<PolicyWatcher> RemotingServiceImpl::CreatePolicyWatcher() {
  return PolicyWatcher::CreateWithPolicyService(
      g_browser_process->policy_service());
}

void RemotingServiceImpl::ReleaseSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_support_host_.reset();
}

}  // namespace

RemotingService& RemotingService::Get() {
  static base::NoDestructor<RemotingServiceImpl> instance;
  return *instance;
}

}  // namespace remoting
