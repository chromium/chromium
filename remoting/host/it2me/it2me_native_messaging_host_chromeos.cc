// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/policy_watcher.h"

namespace remoting {

std::unique_ptr<extensions::NativeMessageHost>
CreateIt2MeNativeMessagingHostForChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> io_runnner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runnner,
    policy::PolicyService* policy_service) {
  std::unique_ptr<It2MeHostFactory> host_factory(new It2MeHostFactory());
  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::CreateForChromeOS(
          io_runnner, ui_runnner,
          base::CreateSingleThreadTaskRunner(
              {base::ThreadPool(), base::MayBlock(),
               base::TaskPriority::BEST_EFFORT}));
  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::CreateWithPolicyService(policy_service);
  std::unique_ptr<extensions::NativeMessageHost> host(
      new It2MeNativeMessagingHost(
          /*needs_elevation=*/false, std::move(policy_watcher),
          std::move(context), std::move(host_factory)));
  return host;
}

}  // namespace remoting
