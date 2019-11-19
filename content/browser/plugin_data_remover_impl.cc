// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_data_remover_impl.h"

#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "build/build_config.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

namespace {

// The minimum Flash Player version that implements NPP_ClearSiteData.
const char kMinFlashVersion[] = "10.3";
const int64_t kRemovalTimeoutMs = 10000;
const uint64_t kClearAllData = 0;

}  // namespace

// static
PluginDataRemover* PluginDataRemover::Create(BrowserContext* browser_context) {
  return new PluginDataRemoverImpl(browser_context);
}

// static
void PluginDataRemover::GetSupportedPlugins(
    std::vector<WebPluginInfo>* supported_plugins) {
  bool allow_wildcard = false;
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), kFlashPluginSwfMimeType, allow_wildcard, &plugins, nullptr);
  base::Version min_version(kMinFlashVersion);
  for (auto it = plugins.begin(); it != plugins.end(); ++it) {
    base::Version version;
    WebPluginInfo::CreateVersionFromString(it->version, &version);
    if (version.IsValid() && min_version.CompareTo(version) == -1)
      supported_plugins->push_back(*it);
  }
}

class PluginDataRemoverImpl::Context
    : public PpapiPluginProcessHost::BrokerClient,
      public IPC::Listener,
      public base::RefCountedThreadSafe<Context,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  Context(base::Time begin_time, BrowserContext* browser_context)
      : event_(new base::WaitableEvent(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED)),
        begin_time_(begin_time),
        is_removing_(false),
        browser_context_path_(browser_context->GetPath()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  void Init(const std::string& mime_type) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&Context::InitOnIOThread, this, mime_type));
    base::PostDelayedTask(FROM_HERE, {BrowserThread::IO},
                          base::BindOnce(&Context::OnTimeout, this),
                          base::TimeDelta::FromMilliseconds(kRemovalTimeoutMs));
  }

  void InitOnIOThread(const std::string& mime_type) {
    PluginServiceImpl* plugin_service = PluginServiceImpl::GetInstance();

    // Get the plugin file path.
    std::vector<WebPluginInfo> plugins;
    plugin_service->GetPluginInfoArray(GURL(), mime_type, false, &plugins,
                                       nullptr);

    if (plugins.empty()) {
      // May be empty for some tests and on the CrOS login OOBE screen.
      event_->Signal();
      return;
    }

    base::FilePath plugin_path = plugins[0].path;

    const PepperPluginInfo* pepper_info =
        plugin_service->GetRegisteredPpapiPluginInfo(plugin_path);
    if (!pepper_info) {
      event_->Signal();
      return;
    }

    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    is_removing_ = true;

    // Balanced in OnPpapiChannelOpened.
    AddRef();
    plugin_name_ = pepper_info->name;
    // Use the broker since we run this function outside the sandbox.
    plugin_service->OpenChannelToPpapiBroker(0, 0, plugin_path, this);
  }

  // Called when a timeout happens in order not to block the client
  // indefinitely.
  void OnTimeout() {
    LOG_IF(ERROR, is_removing_) << "Timed out";
    SignalDone();
  }

  bool Incognito() override { return false; }

  // PpapiPluginProcessHost::BrokerClient implementation.
  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    *renderer_handle = base::kNullProcessHandle;
    *renderer_id = 0;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId /* peer_pid */,
                            int /* child_id */) override {
    if (channel_handle.is_mojo_channel_handle())
      ConnectToChannel(channel_handle);

    // Balancing the AddRef call.
    Release();
  }

  // IPC::Listener methods.
  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(Context, message)
      IPC_MESSAGE_HANDLER(PpapiHostMsg_ClearSiteDataResult,
                          OnPpapiClearSiteDataResult)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()

    return true;
  }

  void OnChannelError() override {
    if (is_removing_) {
      NOTREACHED() << "Channel error";
      SignalDone();
    }
  }

  base::WaitableEvent* event() { return event_.get(); }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<Context>;
  ~Context() override {}

  IPC::Message* CreatePpapiClearSiteDataMsg(uint64_t max_age) {
    base::FilePath profile_path =
        PepperFlashFileMessageFilter::GetDataDirName(browser_context_path_);
    // TODO(vtl): This "duplicates" logic in webkit/plugins/ppapi/file_path.cc
    // (which prepends the plugin name to the relative part of the path
    // instead, with the absolute, profile-dependent part being enforced by
    // the browser).
#if defined(OS_WIN)
    base::FilePath plugin_data_path =
        profile_path.Append(base::FilePath(base::UTF8ToUTF16(plugin_name_)));
#else
    base::FilePath plugin_data_path =
        profile_path.Append(base::FilePath(plugin_name_));
#endif  // defined(OS_WIN)
    return new PpapiMsg_ClearSiteData(0u, plugin_data_path, std::string(),
                                      kClearAllData, max_age);
  }

  // Connects the client side of a newly opened plugin channel.
  void ConnectToChannel(const IPC::ChannelHandle& handle) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // If we timed out, don't bother connecting.
    if (!is_removing_)
      return;

    DCHECK(!channel_.get());
    channel_ = IPC::Channel::CreateClient(handle, this,
                                          base::ThreadTaskRunnerHandle::Get());
    if (!channel_->Connect()) {
      NOTREACHED() << "Couldn't connect to plugin";
      SignalDone();
      return;
    }

    uint64_t max_age = begin_time_.is_null()
                           ? std::numeric_limits<uint64_t>::max()
                           : (base::Time::Now() - begin_time_).InSeconds();

    IPC::Message* msg = CreatePpapiClearSiteDataMsg(max_age);
    if (!channel_->Send(msg)) {
      NOTREACHED() << "Couldn't send ClearSiteData message";
      SignalDone();
      return;
    }
  }

  // Handles the PpapiHostMsg_ClearSiteDataResult message.
  void OnPpapiClearSiteDataResult(uint32_t request_id, bool success) {
    DCHECK_EQ(0u, request_id);
    LOG_IF(ERROR, !success) << "ClearSiteData returned error";
    SignalDone();
  }

  // Signals that we are finished with removing data (successful or not). This
  // method is safe to call multiple times.
  void SignalDone() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (!is_removing_)
      return;
    is_removing_ = false;
    event_->Signal();
  }

  std::unique_ptr<base::WaitableEvent> event_;
  // The point in time from which on we remove data.
  base::Time begin_time_;
  bool is_removing_;

  // Path for the current profile. Must be retrieved on the UI thread from the
  // browser context when we start so we can use it later on the I/O thread.
  base::FilePath browser_context_path_;

  // The name of the plugin. Use only on the I/O thread.
  std::string plugin_name_;

  // The channel is NULL until we have opened a connection to the plugin
  // process.
  std::unique_ptr<IPC::Channel> channel_;
};


PluginDataRemoverImpl::PluginDataRemoverImpl(BrowserContext* browser_context)
    : mime_type_(kFlashPluginSwfMimeType),
      browser_context_(browser_context) {
}

PluginDataRemoverImpl::~PluginDataRemoverImpl() {
}

base::WaitableEvent* PluginDataRemoverImpl::StartRemoving(
    base::Time begin_time) {
  DCHECK(!context_.get());
  context_ = new Context(begin_time, browser_context_);
  context_->Init(mime_type_);
  return context_->event();
}

}  // namespace content
