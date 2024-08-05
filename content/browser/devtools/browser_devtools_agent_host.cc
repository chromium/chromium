// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/clang_profiling_buildflags.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "components/viz/common/buildflags.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/memory_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/storage_handler.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"

#ifdef ENABLE_BLUETOOTH_EMULATION
#include "content/browser/devtools/protocol/bluetooth_emulation_handler.h"
#endif

#if BUILDFLAG(USE_VIZ_DEBUGGER)
#include "content/browser/devtools/protocol/visual_debugger_handler.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
#include "content/browser/devtools/protocol/native_profiling_handler.h"
#endif

namespace content {

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserDevToolsAgentHost(
      tethering_task_runner, socket_callback, false);
}

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForDiscovery() {
  CreateServerSocketCallback null_callback;
  return new BrowserDevToolsAgentHost(nullptr, std::move(null_callback), true);
}

namespace {
std::set<BrowserDevToolsAgentHost*>& BrowserDevToolsAgentHostInstances() {
  static base::NoDestructor<std::set<BrowserDevToolsAgentHost*>> instances;
  return *instances;
}

}  // namespace

class BrowserDevToolsAgentHost::BrowserAutoAttacher final
    : public protocol::TargetAutoAttacher,
      public ServiceWorkerDevToolsManager::Observer,
      public DevToolsAgentHostObserver {
 public:
  BrowserAutoAttacher() = default;
  ~BrowserAutoAttacher() override = default;

 protected:
  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host,
                     bool* should_pause_on_start) override {
    *should_pause_on_start = wait_for_debugger_on_start();
    DispatchAutoAttach(host, *should_pause_on_start);
  }

  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override {
    DispatchAutoDetach(host);
  }

  void ReattachServiceWorkers() {
    DCHECK(auto_attach());
    ServiceWorkerDevToolsAgentHost::List agent_hosts;
    ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&agent_hosts);
    Hosts new_hosts(agent_hosts.begin(), agent_hosts.end());
    DispatchSetAttachedTargetsOfType(new_hosts,
                                     DevToolsAgentHost::kTypeServiceWorker);
  }

  void UpdateAutoAttach(base::OnceClosure callback) override {
    if (auto_attach()) {
      base::AutoReset<bool> auto_reset(&processing_existent_targets_, true);
      if (!have_observers_) {
        ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
        // DevToolsAgentHost's observer immediately notifies about all existing
        // ones.
        DevToolsAgentHost::AddObserver(this);
      } else {
        // Manually collect existing hosts to update the list.
        DevToolsAgentHost::List hosts;
        RenderFrameDevToolsAgentHost::AddAllAgentHosts(&hosts);
        for (auto& host : hosts)
          DevToolsAgentHostCreated(host.get());
      }
      ReattachServiceWorkers();
    } else {
      if (have_observers_) {
        DevToolsAgentHost::RemoveObserver(this);
        ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
      }
    }
    have_observers_ = auto_attach();
    std::move(callback).Run();
  }

  // DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostCreated(DevToolsAgentHost* host) override {
    DCHECK(auto_attach());
    // In the top level target handler, auto-attach to pages as soon as they
    // are created, otherwise if they don't incur any network activity we'll
    // never get a chance to throttle them (and auto-attach there).
    if (ShouldAttachToTarget(host)) {
      DispatchAutoAttach(
          host, wait_for_debugger_on_start() && !processing_existent_targets_);
    }
  }

  bool ShouldForceDevToolsAgentHostCreation() override { return true; }

  static bool ShouldAttachToTarget(DevToolsAgentHost* host) {
    if (host->GetType() == DevToolsAgentHost::kTypeSharedWorker) {
      return true;
    }
    if (host->GetType() == DevToolsAgentHost::kTypeSharedStorageWorklet) {
      return true;
    }
    if (host->GetType() == DevToolsAgentHost::kTypeTab) {
      return true;
    }
    return IsMainFrameHost(host);
  }

  static bool IsMainFrameHost(DevToolsAgentHost* host) {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(host->GetWebContents());
    if (!web_contents)
      return false;
    FrameTreeNode* frame_tree_node = web_contents->GetPrimaryFrameTree().root();
    if (!frame_tree_node)
      return false;
    return host == RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  }

  bool processing_existent_targets_ = false;
  bool have_observers_ = false;
};

// static
const std::set<BrowserDevToolsAgentHost*>&
BrowserDevToolsAgentHost::Instances() {
  return BrowserDevToolsAgentHostInstances();
}

BrowserDevToolsAgentHost::BrowserDevToolsAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback,
    bool only_discovery)
    : DevToolsAgentHostImpl(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      auto_attacher_(std::make_unique<BrowserAutoAttacher>()),
      tethering_task_runner_(tethering_task_runner),
      socket_callback_(socket_callback),
      only_discovery_(only_discovery) {
  NotifyCreated();
  BrowserDevToolsAgentHostInstances().insert(this);
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
  BrowserDevToolsAgentHostInstances().erase(this);
}

bool BrowserDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                             bool acquire_wake_lock) {
  if (!session->GetClient()->IsTrusted())
    return false;

  session->SetBrowserOnly(true);
  session->CreateAndAddHandler<protocol::TargetHandler>(
      protocol::TargetHandler::AccessMode::kBrowser, GetId(),
      auto_attacher_.get(), session);
  if (only_discovery_)
    return true;

#ifdef ENABLE_BLUETOOTH_EMULATION
  session->CreateAndAddHandler<protocol::BluetoothEmulationHandler>();
#endif
  session->CreateAndAddHandler<protocol::BrowserHandler>(
      session->GetClient()->MayWriteLocalFiles());
#if BUILDFLAG(USE_VIZ_DEBUGGER)
  session->CreateAndAddHandler<protocol::VisualDebuggerHandler>();
#endif
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::FetchHandler>(
      GetIOContext(),
      base::BindRepeating([](base::OnceClosure cb) { std::move(cb).Run(); }));
  session->CreateAndAddHandler<protocol::MemoryHandler>();
  session->CreateAndAddHandler<protocol::SecurityHandler>();
  session->CreateAndAddHandler<protocol::StorageHandler>(session->GetClient());
  session->CreateAndAddHandler<protocol::SystemInfoHandler>(
      /* is_browser_sessoin= */ true);
  if (tethering_task_runner_) {
    session->CreateAndAddHandler<protocol::TetheringHandler>(
        socket_callback_, tethering_task_runner_);
  }
  session->CreateAndAddHandler<protocol::TracingHandler>(
      this, GetIOContext(), /* root_session */ nullptr);

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
  session->CreateAndAddHandler<protocol::NativeProfilingHandler>();
#endif

  return true;
}

void BrowserDevToolsAgentHost::DetachSession(DevToolsSession* session) {
}

protocol::TargetAutoAttacher* BrowserDevToolsAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

std::string BrowserDevToolsAgentHost::GetType() {
  return kTypeBrowser;
}

std::string BrowserDevToolsAgentHost::GetTitle() {
  return "";
}

GURL BrowserDevToolsAgentHost::GetURL() {
  return GURL();
}

bool BrowserDevToolsAgentHost::Activate() {
  return false;
}

bool BrowserDevToolsAgentHost::Close() {
  return false;
}

void BrowserDevToolsAgentHost::Reload() {
}

}  // content
