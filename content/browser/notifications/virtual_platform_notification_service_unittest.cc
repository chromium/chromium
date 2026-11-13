// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/virtual_platform_notification_service.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/platform_notification_service_routing.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_platform_notification_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using CloseReason = VirtualPlatformNotificationService::CloseReason;
using Closed = std::vector<std::pair<std::string, CloseReason>>;

const char kOrigin[] = "https://example.com/";
const char kServiceWorkerScope[] = "https://example.com/sw/";
const char kOtherOrigin[] = "https://other.example/";

class TestObserver : public VirtualPlatformNotificationService::Observer {
 public:
  const std::vector<std::string>& shown() const { return shown_; }
  const Closed& closed() const { return closed_; }
  int destroyed_count() const { return destroyed_count_; }

  // VirtualPlatformNotificationService::Observer:
  void OnNotificationShown(
      VirtualPlatformNotificationService* service,
      const VirtualPlatformNotificationService::Notification& notification)
      override {
    shown_.push_back(notification.id);
  }
  void OnNotificationClosed(VirtualPlatformNotificationService* service,
                            const std::string& notification_id,
                            CloseReason reason) override {
    closed_.emplace_back(notification_id, reason);
  }
  void OnServiceDestroyed(
      VirtualPlatformNotificationService* service) override {
    ++destroyed_count_;
  }

 private:
  std::vector<std::string> shown_;
  Closed closed_;
  int destroyed_count_ = 0;
};

// Detaches from the service when a notification is closed, which is not
// allowed.
class DetachingObserver : public TestObserver {
 public:
  void OnNotificationClosed(VirtualPlatformNotificationService* service,
                            const std::string& notification_id,
                            CloseReason reason) override {
    service->Detach(this);
  }
};

class TestNotificationListener
    : public blink::mojom::NonPersistentNotificationListener {
 public:
  mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
  GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  int show_count() const { return show_count_; }
  int click_count() const { return click_count_; }
  int close_count() const { return close_count_; }

  // blink::mojom::NonPersistentNotificationListener:
  void OnShow() override { ++show_count_; }
  void OnClick(OnClickCallback callback) override {
    ++click_count_;
    std::move(callback).Run();
  }
  void OnClose(OnCloseCallback callback) override {
    ++close_count_;
    std::move(callback).Run();
  }

 private:
  int show_count_ = 0;
  int click_count_ = 0;
  int close_count_ = 0;
  mojo::Receiver<blink::mojom::NonPersistentNotificationListener> receiver_{
      this};
};

blink::PlatformNotificationData CreateData(const std::u16string& title,
                                           const std::string& tag = "") {
  blink::PlatformNotificationData data;
  data.title = title;
  data.tag = tag;
  return data;
}

}  // namespace

class VirtualPlatformNotificationServiceTest
    : public RenderViewHostTestHarness {
 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<TestBrowserContext>();
    // Normal contexts put the notification database on the filesystem, which on
    // Windows may block deleting the context directory on exit and thus cause
    // the test to flake. Make it off-the-record to keep data in memory.
    context->set_is_off_the_record(true);
    return context;
  }

  void Display(PlatformNotificationService* service,
               const std::string& id,
               const std::u16string& title = u"Title",
               const std::string& tag = "") {
    service->DisplayNotification(id, GURL(kOrigin), GURL(kOrigin),
                                 CreateData(title, tag),
                                 blink::NotificationResources());
  }

  void DisplayPersistent(PlatformNotificationService* service,
                         const std::string& id,
                         const std::string& tag = "") {
    service->DisplayPersistentNotification(
        id, GURL(kServiceWorkerScope), GURL(kOrigin), CreateData(u"Title", tag),
        blink::NotificationResources());
  }

  void RegisterListener(TestNotificationListener& listener,
                        const std::string& id) {
    NotificationEventDispatcherImpl::GetInstance()
        ->RegisterNonPersistentNotificationListener(
            id, listener.GetRemote(), main_rfh()->GetWeakDocumentPtr(),
            RenderProcessHost::NotificationServiceCreatorType::kDocument);
  }

  TestBrowserContext* test_browser_context() {
    return static_cast<TestBrowserContext*>(browser_context());
  }

  // Sets up an embedder service and returns it.
  PlatformNotificationService* SetEmbedderService() {
    auto mock =
        std::make_unique<MockPlatformNotificationService>(browser_context());
    PlatformNotificationService* embedder_service = mock.get();
    test_browser_context()->SetPlatformNotificationService(std::move(mock));
    return embedder_service;
  }

  void GrantNotificationPermission() {
    base::test::TestFuture<PermissionControllerImpl::OverrideStatus> future;
    PermissionControllerImpl::FromBrowserContext(browser_context())
        ->SetPermissionOverride(url::Origin::Create(GURL(kOrigin)),
                                url::Origin::Create(GURL(kOrigin)),
                                blink::PermissionType::NOTIFICATIONS,
                                blink::mojom::PermissionStatus::GRANTED,
                                future.GetCallback());
    ASSERT_EQ(future.Get(),
              PermissionControllerImpl::OverrideStatus::kOverrideSet);
  }

  std::set<std::string> GetDisplayedNotifications(
      PlatformNotificationService* service) {
    base::test::TestFuture<std::set<std::string>, bool> future;
    service->GetDisplayedNotifications(future.GetCallback());
    return future.Get<0>();
  }
};

TEST_F(VirtualPlatformNotificationServiceTest, RoutesWhileAttached) {
  PlatformNotificationService* embedder_service = SetEmbedderService();
  EXPECT_EQ(GetPlatformNotificationServiceFor(browser_context()),
            embedder_service);

  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  EXPECT_EQ(
      VirtualPlatformNotificationService::FromBrowserContext(browser_context()),
      service);
  EXPECT_EQ(GetPlatformNotificationServiceFor(browser_context()), service);

  service->Detach(&observer);
  EXPECT_FALSE(VirtualPlatformNotificationService::FromBrowserContext(
      browser_context()));
  EXPECT_EQ(GetPlatformNotificationServiceFor(browser_context()),
            embedder_service);
}

TEST_F(VirtualPlatformNotificationServiceTest, ExistsWhileObserversAttached) {
  TestObserver observer1;
  TestObserver observer2;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer1);
  EXPECT_EQ(
      VirtualPlatformNotificationService::Attach(browser_context(), &observer2),
      service);

  service->Detach(&observer1);
  EXPECT_EQ(
      VirtualPlatformNotificationService::FromBrowserContext(browser_context()),
      service);

  service->Detach(&observer2);
  EXPECT_FALSE(VirtualPlatformNotificationService::FromBrowserContext(
      browser_context()));
  EXPECT_EQ(observer1.destroyed_count(), 0);
  EXPECT_EQ(observer2.destroyed_count(), 0);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       ClosesNotificationsWhenLastObserverDetaches) {
  TestObserver observer1;
  TestObserver observer2;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer1);
  VirtualPlatformNotificationService::Attach(browser_context(), &observer2);
  TestNotificationListener listener;
  RegisterListener(listener, "id");
  Display(service, "id");

  service->Detach(&observer1);
  // A close event would have been delivered before the click.
  service->SimulateClick("id", std::nullopt);
  EXPECT_TRUE(
      base::test::RunUntil([&] { return listener.click_count() == 1; }));
  EXPECT_EQ(listener.close_count(), 0);

  service->Detach(&observer2);
  EXPECT_TRUE(
      base::test::RunUntil([&] { return listener.close_count() == 1; }));
}

TEST_F(VirtualPlatformNotificationServiceTest,
       NotifiesObserversWhenContextIsDestroyed) {
  auto context = std::make_unique<TestBrowserContext>();
  TestObserver observer;
  VirtualPlatformNotificationService::Attach(context.get(), &observer);

  context.reset();
  EXPECT_EQ(observer.destroyed_count(), 1);
}

TEST_F(VirtualPlatformNotificationServiceTest, ObserversMustNotReenter) {
  DetachingObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  Display(service, "id");

  EXPECT_CHECK_DEATH(service->CloseNotification("id"));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, DisplayAndClose) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  Display(service, "id");
  ASSERT_TRUE(service->notifications().contains("id"));
  const auto& notification = service->notifications().at("id");
  EXPECT_EQ(notification.origin, GURL(kOrigin));
  EXPECT_FALSE(notification.is_persistent);
  EXPECT_TRUE(notification.service_worker_scope.is_empty());
  EXPECT_EQ(observer.shown(), std::vector<std::string>{"id"});

  service->CloseNotification("id");
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  // Closing an unknown notification is not reported.
  service->CloseNotification("id");
  EXPECT_EQ(observer.closed().size(), 1u);

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, DisplayAndClosePersistent) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  DisplayPersistent(service, "id");
  ASSERT_TRUE(service->notifications().contains("id"));
  const auto& notification = service->notifications().at("id");
  EXPECT_TRUE(notification.is_persistent);
  EXPECT_EQ(notification.service_worker_scope, GURL(kServiceWorkerScope));
  EXPECT_EQ(observer.shown(), std::vector<std::string>{"id"});

  service->ClosePersistentNotification("id");
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       ForwardsClosingEmbedderNotifications) {
  PlatformNotificationService* embedder_service = SetEmbedderService();
  Display(embedder_service, "a");
  DisplayPersistent(embedder_service, "b");
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  service->CloseNotification("a");
  service->ClosePersistentNotification("b");
  EXPECT_TRUE(GetDisplayedNotifications(embedder_service).empty());
  EXPECT_TRUE(observer.closed().empty());

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       ClosesReplacedPersistentEmbedderNotification) {
  PlatformNotificationService* embedder_service = SetEmbedderService();
  Display(embedder_service, "a", u"Title", "tag");
  DisplayPersistent(embedder_service, "b", "tag");
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  Display(service, "a", u"Title", "tag");
  DisplayPersistent(service, "b", "tag");
  // Only the persistent one is closed.
  EXPECT_EQ(GetDisplayedNotifications(embedder_service),
            std::set<std::string>{"a"});
  EXPECT_EQ(service->notifications().size(), 2u);

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, ReplacesNotificationWithSameId) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  Display(service, "id", u"First");
  Display(service, "id", u"Second");
  ASSERT_EQ(service->notifications().size(), 1u);
  EXPECT_EQ(service->notifications().at("id").data.title, u"Second");
  EXPECT_EQ(observer.shown(), (std::vector<std::string>{"id", "id"}));
  EXPECT_TRUE(observer.closed().empty());

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, GetDisplayedNotifications) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  Display(service, "a");
  DisplayPersistent(service, "b");

  base::test::TestFuture<std::set<std::string>, bool> future;
  service->GetDisplayedNotifications(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), (std::set<std::string>{"a", "b"}));
  // Without an embedder service, synchronization is supported.
  EXPECT_TRUE(future.Get<1>());

  SetEmbedderService();
  base::test::TestFuture<std::set<std::string>, bool> embedder_future;
  service->GetDisplayedNotifications(embedder_future.GetCallback());
  EXPECT_FALSE(embedder_future.Get<1>());

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       GetDisplayedNotificationsForOrigin) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  Display(service, "a");
  DisplayPersistent(service, "b");
  service->DisplayNotification("c", GURL(kOtherOrigin), GURL(kOtherOrigin),
                               CreateData(u"Title"),
                               blink::NotificationResources());

  base::test::TestFuture<std::set<std::string>, bool> future;
  service->GetDisplayedNotificationsForOrigin(GURL(kOrigin),
                                              future.GetCallback());
  EXPECT_EQ(future.Get<0>(), (std::set<std::string>{"a", "b"}));
  EXPECT_TRUE(future.Get<1>());

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, SimulateUnknownNotification) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  service->SimulateClick("unknown", std::nullopt);
  service->SimulateClose("unknown");
  EXPECT_TRUE(observer.closed().empty());

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, SimulateClickAndClose) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  TestNotificationListener listener;
  RegisterListener(listener, "id");

  Display(service, "id");
  EXPECT_TRUE(base::test::RunUntil([&] { return listener.show_count() == 1; }));

  service->SimulateClick("id", std::nullopt);
  EXPECT_TRUE(
      base::test::RunUntil([&] { return listener.click_count() == 1; }));
  EXPECT_TRUE(service->notifications().contains("id"));

  service->SimulateClose("id");
  EXPECT_TRUE(
      base::test::RunUntil([&] { return listener.close_count() == 1; }));
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kClient}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, SimulateClickChecksActionIndex) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  Display(service, "non-persistent");
  DisplayPersistent(service, "persistent");

  EXPECT_CHECK_DEATH(service->SimulateClick("non-persistent", 0));
  EXPECT_CHECK_DEATH(service->SimulateClick("persistent", -1));
  // The notification has no actions.
  EXPECT_CHECK_DEATH(service->SimulateClick("persistent", 0));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       SimulateClickWithoutListenerClosesNotification) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  Display(service, "id");

  service->SimulateClick("id", std::nullopt);
  EXPECT_TRUE(base::test::RunUntil([&] { return !observer.closed().empty(); }));
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       SimulateClickWithoutPermissionClosesNotification) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  // The test browser context does not grant the permission.
  DisplayPersistent(service, "id");

  service->SimulateClick("id", std::nullopt);
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       SimulateClickOnPersistentNotification) {
  GrantNotificationPermission();
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  DisplayPersistent(service, "id");

  service->SimulateClick("id", std::nullopt);
  // The click is dispatched, unlike without the permission.
  EXPECT_TRUE(service->notifications().contains("id"));
  // There is no database entry, so the dispatch fails and the notification is
  // closed.
  EXPECT_TRUE(base::test::RunUntil([&] { return !observer.closed().empty(); }));
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       SimulateActionClickOnPersistentNotification) {
  GrantNotificationPermission();
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  blink::PlatformNotificationData data = CreateData(u"Title");
  auto action = blink::mojom::NotificationAction::New();
  action->type = blink::mojom::NotificationActionType::BUTTON;
  action->action = "action";
  action->title = u"Action";
  data.actions.push_back(std::move(action));
  service->DisplayPersistentNotification("id", GURL(kServiceWorkerScope),
                                         GURL(kOrigin), data,
                                         blink::NotificationResources());

  service->SimulateClick("id", 0);
  EXPECT_TRUE(service->notifications().contains("id"));
  EXPECT_TRUE(base::test::RunUntil([&] { return !observer.closed().empty(); }));
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kOther}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       SimulateCloseOnPersistentNotification) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  DisplayPersistent(service, "id");

  service->SimulateClose("id");
  EXPECT_TRUE(service->notifications().empty());
  EXPECT_EQ(observer.closed(), (Closed{{"id", CloseReason::kClient}}));

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest, PersistentNotificationIds) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);

  // Without an embedder service, the virtual one keeps its own counter.
  int64_t first = service->ReadNextPersistentNotificationId();
  EXPECT_EQ(service->ReadNextPersistentNotificationId(), first + 1);

  // With one, its counter is shared so that IDs do not collide.
  PlatformNotificationService* embedder_service = SetEmbedderService();
  int64_t id = service->ReadNextPersistentNotificationId();
  EXPECT_EQ(embedder_service->ReadNextPersistentNotificationId(), id + 1);

  service->Detach(&observer);
}

TEST_F(VirtualPlatformNotificationServiceTest,
       PersistentNotificationIdsSurviveService) {
  TestObserver observer;
  auto* service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  int64_t id = service->ReadNextPersistentNotificationId();
  service->Detach(&observer);
  ASSERT_FALSE(VirtualPlatformNotificationService::FromBrowserContext(
      browser_context()));

  service =
      VirtualPlatformNotificationService::Attach(browser_context(), &observer);
  EXPECT_EQ(service->ReadNextPersistentNotificationId(), id + 1);

  service->Detach(&observer);
}

}  // namespace content
