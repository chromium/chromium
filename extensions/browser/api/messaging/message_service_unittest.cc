// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_task_queue_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace {

constexpr char kAllowlistedExtensionId[] = "khpfeaanjngmcnplbdlpegiifgpfgdco";
constexpr char kOtherExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kTargetExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class TestExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<TestExtensionWebContentsObserver> {
 public:
  ~TestExtensionWebContentsObserver() override = default;

 private:
  friend class content::WebContentsUserData<TestExtensionWebContentsObserver>;

  explicit TestExtensionWebContentsObserver(content::WebContents* web_contents)
      : ExtensionWebContentsObserver(web_contents),
        content::WebContentsUserData<TestExtensionWebContentsObserver>(
            *web_contents) {
    Initialize();
  }

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TestExtensionWebContentsObserver);

class MessagingTestExtensionsBrowserClient
    : public TestExtensionsBrowserClient {
 public:
  MessagingTestExtensionsBrowserClient() = default;
  ~MessagingTestExtensionsBrowserClient() override = default;

  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override {
    return TestExtensionWebContentsObserver::FromWebContents(web_contents);
  }
};

class TestMessagingDelegate : public MessagingDelegate {
 public:
  TestMessagingDelegate() = default;
  ~TestMessagingDelegate() override = default;

  void set_tab_info(std::optional<base::DictValue> tab_info) {
    tab_info_ = std::move(tab_info);
  }

  std::optional<base::DictValue> MaybeGetTabInfo(
      content::WebContents* web_contents) override {
    if (!web_contents || !tab_info_) {
      return std::nullopt;
    }
    return tab_info_->Clone();
  }

 private:
  std::optional<base::DictValue> tab_info_;
};

class TestExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() = default;
  ~TestExtensionsAPIClient() override = default;

  MessagingDelegate* GetMessagingDelegate() override {
    return &messaging_delegate_;
  }
  TestMessagingDelegate& messaging_delegate() { return messaging_delegate_; }

 private:
  TestMessagingDelegate messaging_delegate_;
};

class ControllableServiceWorkerTaskQueue : public ServiceWorkerTaskQueue {
 public:
  explicit ControllableServiceWorkerTaskQueue(content::BrowserContext* context)
      : ServiceWorkerTaskQueue(context) {}
  ~ControllableServiceWorkerTaskQueue() override = default;

  void set_defer_tasks(bool defer_tasks) { defer_tasks_ = defer_tasks; }
  size_t queued_task_count() const { return queued_tasks_.size(); }

  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension) const override {
    return defer_tasks_ ||
           ServiceWorkerTaskQueue::ShouldEnqueueTask(context, extension);
  }

  void AddPendingTask(const LazyContextId& context_id,
                      PendingTask task) override {
    if (defer_tasks_) {
      queued_tasks_.push_back(std::move(task));
      return;
    }
    std::move(task).Run(nullptr);
  }

  void FlushWithNullContextInfo() {
    auto tasks = std::move(queued_tasks_);
    queued_tasks_.clear();
    for (auto& task : tasks) {
      std::move(task).Run(nullptr);
    }
  }

 private:
  bool defer_tasks_ = false;
  std::vector<PendingTask> queued_tasks_;
};

std::unique_ptr<KeyedService> BuildControllableServiceWorkerQueue(
    content::BrowserContext* context) {
  return std::make_unique<ControllableServiceWorkerTaskQueue>(context);
}

class TestProcessManager : public ProcessManager {
 public:
  explicit TestProcessManager(content::BrowserContext* context)
      : ProcessManager(context, ExtensionRegistry::Get(context)) {}
  ~TestProcessManager() override = default;

  bool CreateBackgroundHost(const Extension* extension,
                            const GURL& url) override {
    return false;
  }
};

std::unique_ptr<KeyedService> BuildTestProcessManager(
    content::BrowserContext* context) {
  return std::make_unique<TestProcessManager>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<EventRouter>(context, ExtensionPrefs::Get(context));
}

class FakeLocalFrame : public mojom::LocalFrame {
 public:
  FakeLocalFrame() = default;
  ~FakeLocalFrame() override = default;

  mojo::PendingAssociatedRemote<mojom::LocalFrame> BindNewEndpoint() {
    receiver_.reset();
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

  int connect_count() const { return connect_count_; }
  const base::DictValue& last_source_tab() const { return last_source_tab_; }

  // mojom::LocalFrame:
  void SetFrameName(const std::string& frame_name) override {}
  void SetSpatialNavigationEnabled(bool spatial_nav_enabled) override {}
  void SetTabId(int32_t tab_id) override {}
  void AppWindowClosed(bool send_onclosed) override {}
  void NotifyRenderViewType(mojom::ViewType view_type) override {}
  void MessageInvoke(const ExtensionId& extension_id,
                     const std::string& module_name,
                     const std::string& function_name,
                     base::ListValue args) override {}
  void ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                   ExecuteCodeCallback callback) override {}
  void ExecuteDeclarativeScript(int32_t tab_id,
                                const ExtensionId& extension_id,
                                const std::string& script_id,
                                const GURL& url) override {}
  void UpdateBrowserWindowId(int32_t window_id) override {}
  void DispatchOnConnect(
      const PortId& port_id,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojom::TabConnectionInfoPtr tab_info,
      mojom::ExternalConnectionInfoPtr external_connection_info,
      mojo::PendingAssociatedReceiver<mojom::MessagePort> port,
      mojo::PendingAssociatedRemote<mojom::MessagePortHost> port_host,
      DispatchOnConnectCallback callback) override {
    ++connect_count_;
    last_source_tab_ = std::move(tab_info->tab);
    held_ports_.push_back(std::move(port));
    held_port_hosts_.push_back(std::move(port_host));
    std::move(callback).Run(/*success=*/true);
  }

 private:
  mojo::AssociatedReceiver<mojom::LocalFrame> receiver_{this};
  int connect_count_ = 0;
  base::DictValue last_source_tab_;
  std::vector<mojo::PendingAssociatedReceiver<mojom::MessagePort>> held_ports_;
  std::vector<mojo::PendingAssociatedRemote<mojom::MessagePortHost>>
      held_port_hosts_;
};

class RecordingOpenerPort : public MessagePort {
 public:
  struct State {
    int disconnect_count = 0;
    std::string last_disconnect_error;
  };

  RecordingOpenerPort(base::WeakPtr<ChannelDelegate> delegate,
                      const PortId& port_id,
                      State* state)
      : MessagePort(std::move(delegate), port_id), state_(state) {}
  ~RecordingOpenerPort() override = default;

  bool IsValidPort() override { return true; }
  void DispatchOnMessage(Message message) override {}
  void DispatchOnDisconnect(const std::string& error_message) override {
    ++state_->disconnect_count;
    state_->last_disconnect_error = error_message;
  }

 private:
  raw_ptr<State> state_;
};

PortId MakeOpenerPortId(int port_number) {
  return PortId(base::UnguessableToken::Create(), port_number,
                /*is_opener=*/true, mojom::SerializationFormat::kJson);
}

}  // namespace

class MessageServiceTest : public ExtensionsTest {
 public:
  MessageServiceTest() {
    content::IsolateAllSitesForTesting(
        scoped_command_line_.GetProcessCommandLine());
    SetExtensionsBrowserClient(
        std::make_unique<MessagingTestExtensionsBrowserClient>());
  }
  ~MessageServiceTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    user_prefs::UserPrefs::Set(browser_context(), pref_service());
    user_prefs::UserPrefs::Set(incognito_context(), pref_service());
    api_client_ = std::make_unique<TestExtensionsAPIClient>();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));
    process_manager_ = static_cast<TestProcessManager*>(
        ProcessManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context(), base::BindRepeating(&BuildTestProcessManager)));
    incognito_process_manager_ = static_cast<TestProcessManager*>(
        ProcessManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            incognito_context(),
            base::BindRepeating(&BuildTestProcessManager)));
    lazy_queue_ = static_cast<ControllableServiceWorkerTaskQueue*>(
        ServiceWorkerTaskQueueFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context(),
            base::BindRepeating(&BuildControllableServiceWorkerQueue)));
    incognito_lazy_queue_ = static_cast<ControllableServiceWorkerTaskQueue*>(
        ServiceWorkerTaskQueueFactory::GetInstance()->SetTestingFactoryAndUse(
            incognito_context(),
            base::BindRepeating(&BuildControllableServiceWorkerQueue)));
    message_service_ = std::make_unique<MessageService>(browser_context());
  }

  void TearDown() override {
    message_service_.reset();
    opener_states_.clear();
    incognito_lazy_queue_ = nullptr;
    lazy_queue_ = nullptr;
    incognito_process_manager_ = nullptr;
    process_manager_ = nullptr;
    api_client_.reset();
    ExtensionsTest::TearDown();
  }

  scoped_refptr<const Extension> AddSimpleExtension(const std::string& id) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("Simple").SetID(id).Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

  scoped_refptr<const Extension> AddLazyServiceWorkerExtension(
      const std::string& id,
      bool split_incognito = false) {
    ExtensionBuilder builder("Lazy");
    builder.SetManifestVersion(3)
        .SetBackgroundContext(
            ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
        .SetID(id);
    if (split_incognito) {
      builder.SetManifestKey("incognito", "split");
    }
    scoped_refptr<const Extension> extension = builder.Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

  std::unique_ptr<content::WebContents> CreateRegisteredExtensionFrameInContext(
      content::BrowserContext* context,
      const Extension* extension,
      FakeLocalFrame* fake_local_frame) {
    const GURL extension_url = extension->GetResourceURL("page.html");
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::CreateForURL(context, extension_url);
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(context,
                                                          site_instance);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(extension_url);
    content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
    content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
    TestExtensionWebContentsObserver::CreateForWebContents(web_contents.get());
    auto* observer =
        ExtensionWebContentsObserver::GetForWebContents(web_contents.get());
    if (fake_local_frame) {
      observer->SetLocalFrameForTesting(rfh,
                                        fake_local_frame->BindNewEndpoint());
    }
    ProcessMap::Get(context)->Insert(extension->id(),
                                     rfh->GetProcess()->GetID());
    ProcessManager::Get(context)->RegisterRenderFrameHost(rfh, extension);
    return web_contents;
  }

  std::unique_ptr<content::WebContents> CreateRegisteredExtensionFrame(
      const Extension* extension,
      FakeLocalFrame* fake_local_frame) {
    return CreateRegisteredExtensionFrameInContext(browser_context(), extension,
                                                   fake_local_frame);
  }

  ChannelEndpoint MakeFrameEndpoint(content::WebContents* web_contents) {
    content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
    return ChannelEndpoint(web_contents->GetBrowserContext(),
                           rfh->GetProcess()->GetDeprecatedID(),
                           PortContext::ForFrame(rfh->GetRoutingID()));
  }

  RecordingOpenerPort::State* CreateOpenerState() {
    opener_states_.push_back(std::make_unique<RecordingOpenerPort::State>());
    return opener_states_.back().get();
  }

  TestMessagingDelegate& messaging_delegate() {
    return api_client_->messaging_delegate();
  }
  TestProcessManager* process_manager() { return process_manager_; }
  ControllableServiceWorkerTaskQueue* lazy_queue() { return lazy_queue_; }
  ControllableServiceWorkerTaskQueue* incognito_lazy_queue() {
    return incognito_lazy_queue_;
  }
  MessageService* message_service() { return message_service_.get(); }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  std::unique_ptr<TestExtensionsAPIClient> api_client_;
  raw_ptr<TestProcessManager> process_manager_ = nullptr;
  raw_ptr<TestProcessManager> incognito_process_manager_ = nullptr;
  raw_ptr<ControllableServiceWorkerTaskQueue> lazy_queue_ = nullptr;
  raw_ptr<ControllableServiceWorkerTaskQueue> incognito_lazy_queue_ = nullptr;
  std::vector<std::unique_ptr<RecordingOpenerPort::State>> opener_states_;
  std::unique_ptr<MessageService> message_service_;
};

TEST_F(MessageServiceTest,
       DeferredLazyReceiverAvoidsSpuriousCloseWhenOtherFrameCloses) {
  scoped_refptr<const Extension> opener_extension =
      AddSimpleExtension(kOtherExtensionId);
  scoped_refptr<const Extension> lazy_extension =
      AddLazyServiceWorkerExtension(kTargetExtensionId);
  lazy_queue()->set_defer_tasks(true);

  FakeLocalFrame opener_frame;
  std::unique_ptr<content::WebContents> opener_contents =
      CreateRegisteredExtensionFrame(opener_extension.get(), &opener_frame);
  FakeLocalFrame transient_frame;
  std::unique_ptr<content::WebContents> transient_contents =
      CreateRegisteredExtensionFrame(lazy_extension.get(), &transient_frame);

  RecordingOpenerPort::State* opener_state = CreateOpenerState();
  const PortId opener_port_id = MakeOpenerPortId(1);
  auto opener_port = std::make_unique<RecordingOpenerPort>(
      message_service()->GetChannelDelegate(), opener_port_id, opener_state);

  message_service()->OpenChannelToExtension(
      MakeFrameEndpoint(opener_contents.get()), opener_port_id,
      MessagingEndpoint::ForExtension(opener_extension->id()),
      std::move(opener_port), lazy_extension->id(),
      Extension::GetBaseURLFromExtensionId(opener_extension->id()),
      mojom::ChannelType::kConnect, "lazy_channel");

  EXPECT_TRUE(message_service()->HasPendingLazyContextChannelsForExtension(
      lazy_extension->id()));
  EXPECT_EQ(1u, lazy_queue()->queued_task_count());
  EXPECT_EQ(0u, message_service()->GetChannelCountForTest());

  // Close the unrelated extension frame while the lazy context is still waking.
  // Because receiver port creation was deferred, closing this frame must not
  // enqueue a spurious `PendingLazyContextClosePort` task.
  process_manager()->UnregisterRenderFrameHost(
      transient_contents->GetPrimaryMainFrame());
  transient_contents.reset();
  EXPECT_EQ(1u, lazy_queue()->queued_task_count());

  // Register a live frame and flush the pending lazy open task.
  FakeLocalFrame live_frame;
  std::unique_ptr<content::WebContents> live_contents =
      CreateRegisteredExtensionFrame(lazy_extension.get(), &live_frame);

  lazy_queue()->set_defer_tasks(false);
  lazy_queue()->FlushWithNullContextInfo();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return live_frame.connect_count() == 1; }));

  EXPECT_FALSE(message_service()->HasPendingLazyContextChannelsForExtension(
      lazy_extension->id()));
  EXPECT_EQ(1u, message_service()->GetChannelCountForTest());
  EXPECT_EQ(0, opener_state->disconnect_count);
}

TEST_F(MessageServiceTest, LazyWakeFailureDisconnectsOrFallsBackToLiveFrame) {
  scoped_refptr<const Extension> opener_extension =
      AddSimpleExtension(kOtherExtensionId);
  scoped_refptr<const Extension> lazy_extension =
      AddLazyServiceWorkerExtension(kTargetExtensionId);
  lazy_queue()->set_defer_tasks(false);

  FakeLocalFrame opener_frame;
  std::unique_ptr<content::WebContents> opener_contents =
      CreateRegisteredExtensionFrame(opener_extension.get(), &opener_frame);

  // Case 1: No live extension frames exist -> opener is disconnected cleanly.
  {
    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(1);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);

    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), lazy_extension->id(),
        Extension::GetBaseURLFromExtensionId(opener_extension->id()),
        mojom::ChannelType::kConnect, "no_frames_channel");

    EXPECT_EQ(1, opener_state->disconnect_count);
    EXPECT_EQ("Could not establish connection. Receiving end does not exist.",
              opener_state->last_disconnect_error);
    EXPECT_FALSE(message_service()->HasPendingLazyContextChannelsForExtension(
        lazy_extension->id()));
    EXPECT_EQ(0u, message_service()->GetChannelCountForTest());
  }

  // Case 2: A live extension frame (e.g. popup/tab) exists -> wake failure
  // still connects to the live frame instead of dropping the channel.
  FakeLocalFrame live_frame;
  std::unique_ptr<content::WebContents> live_contents =
      CreateRegisteredExtensionFrame(lazy_extension.get(), &live_frame);

  {
    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(2);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);

    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), lazy_extension->id(),
        Extension::GetBaseURLFromExtensionId(opener_extension->id()),
        mojom::ChannelType::kConnect, "live_frame_fallback_channel");
    ASSERT_TRUE(
        base::test::RunUntil([&] { return live_frame.connect_count() == 1; }));

    EXPECT_EQ(0, opener_state->disconnect_count);
    EXPECT_FALSE(message_service()->HasPendingLazyContextChannelsForExtension(
        lazy_extension->id()));
    EXPECT_EQ(1u, message_service()->GetChannelCountForTest());
  }

  // Case 3: Opener frame becomes invalid while queued -> wake failure skips
  // receiver creation and cleans up without connecting to `live_frame`.
  {
    FakeLocalFrame transient_opener_frame;
    std::unique_ptr<content::WebContents> transient_opener_contents =
        CreateRegisteredExtensionFrame(opener_extension.get(),
                                       &transient_opener_frame);
    lazy_queue()->set_defer_tasks(true);

    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(3);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);

    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(transient_opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), lazy_extension->id(),
        Extension::GetBaseURLFromExtensionId(opener_extension->id()),
        mojom::ChannelType::kConnect, "invalidated_opener_channel");
    EXPECT_EQ(1u, lazy_queue()->queued_task_count());

    process_manager()->UnregisterRenderFrameHost(
        transient_opener_contents->GetPrimaryMainFrame());
    transient_opener_contents.reset();

    lazy_queue()->set_defer_tasks(false);
    lazy_queue()->FlushWithNullContextInfo();

    EXPECT_EQ(1, live_frame.connect_count());
    EXPECT_FALSE(message_service()->HasPendingLazyContextChannelsForExtension(
        lazy_extension->id()));
    EXPECT_EQ(1u, message_service()->GetChannelCountForTest());
  }

  // Case 4: Target extension is disabled while queued -> wake failure does not
  // connect to `live_frame` and disconnects the opener cleanly.
  {
    lazy_queue()->set_defer_tasks(true);

    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(4);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);

    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), lazy_extension->id(),
        Extension::GetBaseURLFromExtensionId(opener_extension->id()),
        mojom::ChannelType::kConnect, "disabled_target_channel");
    EXPECT_EQ(1u, lazy_queue()->queued_task_count());

    ExtensionRegistry::Get(browser_context())
        ->RemoveEnabled(lazy_extension->id());

    lazy_queue()->set_defer_tasks(false);
    lazy_queue()->FlushWithNullContextInfo();

    EXPECT_EQ(1, live_frame.connect_count());
    EXPECT_EQ(1, opener_state->disconnect_count);
    EXPECT_EQ("Could not establish connection. Receiving end does not exist.",
              opener_state->last_disconnect_error);
    EXPECT_FALSE(message_service()->HasPendingLazyContextChannelsForExtension(
        lazy_extension->id()));
    EXPECT_EQ(1u, message_service()->GetChannelCountForTest());
  }
}

TEST_F(MessageServiceTest,
       LazyWakeFailureSplitModeIncognitoFallsBackOnlyToIncognitoFrames) {
  scoped_refptr<const Extension> opener_extension =
      AddSimpleExtension(kOtherExtensionId);
  scoped_refptr<const Extension> split_extension =
      AddLazyServiceWorkerExtension(kTargetExtensionId,
                                    /*split_incognito=*/true);
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(opener_extension->id(), true);
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(split_extension->id(), true);
  incognito_lazy_queue()->set_defer_tasks(false);

  FakeLocalFrame incognito_opener_frame;
  std::unique_ptr<content::WebContents> incognito_opener_contents =
      CreateRegisteredExtensionFrameInContext(
          incognito_context(), opener_extension.get(), &incognito_opener_frame);

  // Register a live frame only in the regular profile. Because the target
  // extension uses split incognito mode, an incognito opener must not fall back
  // to the regular-profile frame when the incognito worker fails to wake.
  FakeLocalFrame regular_live_frame;
  std::unique_ptr<content::WebContents> regular_live_contents =
      CreateRegisteredExtensionFrameInContext(
          browser_context(), split_extension.get(), &regular_live_frame);

  {
    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(1);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);

    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(incognito_opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), split_extension->id(),
        Extension::GetBaseURLFromExtensionId(opener_extension->id()),
        mojom::ChannelType::kConnect, "split_incognito_isolated_channel");

    EXPECT_EQ(0, regular_live_frame.connect_count());
    EXPECT_EQ(1, opener_state->disconnect_count);
    EXPECT_EQ("Could not establish connection. Receiving end does not exist.",
              opener_state->last_disconnect_error);
    EXPECT_EQ(0u, message_service()->GetChannelCountForTest());
  }

  // Registering a live frame in the incognito context allows the split-mode
  // incognito wake failure to fall back to that incognito frame.
  FakeLocalFrame incognito_live_frame;
  std::unique_ptr<content::WebContents> incognito_live_contents =
      CreateRegisteredExtensionFrameInContext(
          incognito_context(), split_extension.get(), &incognito_live_frame);

  RecordingOpenerPort::State* opener_state = CreateOpenerState();
  const PortId opener_port_id = MakeOpenerPortId(2);
  auto opener_port = std::make_unique<RecordingOpenerPort>(
      message_service()->GetChannelDelegate(), opener_port_id, opener_state);

  message_service()->OpenChannelToExtension(
      MakeFrameEndpoint(incognito_opener_contents.get()), opener_port_id,
      MessagingEndpoint::ForExtension(opener_extension->id()),
      std::move(opener_port), split_extension->id(),
      Extension::GetBaseURLFromExtensionId(opener_extension->id()),
      mojom::ChannelType::kConnect, "split_incognito_fallback_channel");
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return incognito_live_frame.connect_count() == 1; }));

  EXPECT_EQ(0, regular_live_frame.connect_count());
  EXPECT_EQ(0, opener_state->disconnect_count);
  EXPECT_EQ(1u, message_service()->GetChannelCountForTest());
}

TEST_F(MessageServiceTest,
       ExtendedLifetimeAllowlistHandlesDirectBlobFileSystemAndPolicyUrls) {
  scoped_refptr<const Extension> opener_extension =
      AddSimpleExtension(kOtherExtensionId);
  scoped_refptr<const Extension> target_extension =
      AddSimpleExtension(kTargetExtensionId);
  FakeLocalFrame opener_frame;
  std::unique_ptr<content::WebContents> opener_contents =
      CreateRegisteredExtensionFrame(opener_extension.get(), &opener_frame);
  FakeLocalFrame target_frame;
  std::unique_ptr<content::WebContents> target_contents =
      CreateRegisteredExtensionFrame(target_extension.get(), &target_frame);

  auto open_from_url = [&](int port_num, const GURL& source_url,
                           const Extension* receiver_ext) {
    RecordingOpenerPort::State* opener_state = CreateOpenerState();
    const PortId opener_port_id = MakeOpenerPortId(port_num);
    auto opener_port = std::make_unique<RecordingOpenerPort>(
        message_service()->GetChannelDelegate(), opener_port_id, opener_state);
    message_service()->OpenChannelToExtension(
        MakeFrameEndpoint(opener_contents.get()), opener_port_id,
        MessagingEndpoint::ForExtension(opener_extension->id()),
        std::move(opener_port), receiver_ext->id(), source_url,
        mojom::ChannelType::kConnect, "keepalive_channel");
    return opener_port_id.GetChannelId();
  };

  // Direct allowlisted chrome-extension:// URL.
  ChannelId direct_id = open_from_url(
      1, GURL("chrome-extension://khpfeaanjngmcnplbdlpegiifgpfgdco/page.html"),
      target_extension.get());
  EXPECT_TRUE(
      message_service()->HasStrongKeepaliveOnReceiverForTest(direct_id));
  EXPECT_FALSE(message_service()->HasStrongKeepaliveOnOpenerForTest(direct_id));

  // Nested blob:chrome-extension:// URL for an allowlisted extension.
  ChannelId blob_id = open_from_url(
      2,
      GURL("blob:chrome-extension://khpfeaanjngmcnplbdlpegiifgpfgdco/uuid-1"),
      target_extension.get());
  EXPECT_TRUE(message_service()->HasStrongKeepaliveOnReceiverForTest(blob_id));

  // Nested filesystem:chrome-extension:// URL for an allowlisted extension.
  ChannelId fs_id = open_from_url(
      3,
      GURL("filesystem:chrome-extension://khpfeaanjngmcnplbdlpegiifgpfgdco/"
           "temporary/file.txt"),
      target_extension.get());
  EXPECT_TRUE(message_service()->HasStrongKeepaliveOnReceiverForTest(fs_id));

  // Non-allowlisted extension URL and non-extension blob URL do not grant
  // strong keepalive.
  ChannelId non_allowlisted_id = open_from_url(
      4, GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/page.html"),
      target_extension.get());
  EXPECT_FALSE(message_service()->HasStrongKeepaliveOnReceiverForTest(
      non_allowlisted_id));

  ChannelId web_blob_id = open_from_url(
      5, GURL("blob:https://example.com/uuid-2"), target_extension.get());
  EXPECT_FALSE(
      message_service()->HasStrongKeepaliveOnReceiverForTest(web_blob_id));

  // Connecting TO an allowlisted target extension grants strong keepalive to
  // the opener port.
  scoped_refptr<const Extension> allowlisted_target =
      AddSimpleExtension(kAllowlistedExtensionId);
  FakeLocalFrame allowlisted_frame;
  std::unique_ptr<content::WebContents> allowlisted_contents =
      CreateRegisteredExtensionFrame(allowlisted_target.get(),
                                     &allowlisted_frame);
  ChannelId target_allowlisted_id = open_from_url(
      6, GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/page.html"),
      allowlisted_target.get());
  EXPECT_TRUE(message_service()->HasStrongKeepaliveOnOpenerForTest(
      target_allowlisted_id));

  // Policy-configured origin grants strong keepalive.
  pref_service()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      base::ListValue().Append(
          "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"));
  ChannelId policy_id = open_from_url(
      7, GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/page.html"),
      target_extension.get());
  EXPECT_TRUE(
      message_service()->HasStrongKeepaliveOnReceiverForTest(policy_id));
}

TEST_F(MessageServiceTest, DispatchOnConnectMovesSourceTabDictionary) {
  scoped_refptr<const Extension> opener_extension =
      AddSimpleExtension(kOtherExtensionId);
  scoped_refptr<const Extension> target_extension =
      AddSimpleExtension(kTargetExtensionId);

  FakeLocalFrame opener_frame;
  std::unique_ptr<content::WebContents> opener_contents =
      CreateRegisteredExtensionFrame(opener_extension.get(), &opener_frame);
  FakeLocalFrame target_frame_1;
  std::unique_ptr<content::WebContents> target_contents_1 =
      CreateRegisteredExtensionFrame(target_extension.get(), &target_frame_1);
  FakeLocalFrame target_frame_2;
  std::unique_ptr<content::WebContents> target_contents_2 =
      CreateRegisteredExtensionFrame(target_extension.get(), &target_frame_2);

  base::DictValue source_tab;
  source_tab.Set("id", 42);
  source_tab.Set("url", "https://example.com/tab");
  messaging_delegate().set_tab_info(std::move(source_tab));

  content::RenderFrameHost* opener_rfh = opener_contents->GetPrimaryMainFrame();
  ChannelEndpoint source(browser_context(),
                         opener_rfh->GetProcess()->GetDeprecatedID(),
                         PortContext::ForFrame(opener_rfh->GetRoutingID()));

  RecordingOpenerPort::State* opener_state = CreateOpenerState();
  const PortId opener_port_id = MakeOpenerPortId(1);
  auto opener_port = std::make_unique<RecordingOpenerPort>(
      message_service()->GetChannelDelegate(), opener_port_id, opener_state);

  message_service()->OpenChannelToExtension(
      source, opener_port_id,
      MessagingEndpoint::ForExtension(opener_extension->id()),
      std::move(opener_port), target_extension->id(),
      Extension::GetBaseURLFromExtensionId(opener_extension->id()),
      mojom::ChannelType::kConnect, "tab_info_channel");
  ASSERT_TRUE(base::test::RunUntil([&] {
    return target_frame_1.connect_count() == 1 &&
           target_frame_2.connect_count() == 1;
  }));

  EXPECT_EQ(0, opener_state->disconnect_count);
  for (const FakeLocalFrame* frame : {&target_frame_1, &target_frame_2}) {
    EXPECT_EQ(42, frame->last_source_tab().FindInt("id"));
    const std::string* url = frame->last_source_tab().FindString("url");
    ASSERT_NE(nullptr, url);
    EXPECT_EQ("https://example.com/tab", *url);
  }
}

}  // namespace extensions
