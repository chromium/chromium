// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_device_bound_session_manager_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/mock_session_service.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_params.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

using net::device_bound_sessions::DeletionReason;
using net::device_bound_sessions::Session;
using net::device_bound_sessions::SessionError;
using net::device_bound_sessions::SessionEvent;
using net::device_bound_sessions::SessionKey;
using net::device_bound_sessions::SessionParams;
using net::device_bound_sessions::SessionServiceMock;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;

using SessionKeysCallback =
    base::OnceCallback<void(const std::vector<SessionKey>&)>;
using SessionsFuture =
    base::test::TestFuture<std::vector<mojom::RestrictedDeviceBoundSessionPtr>>;

constexpr char kSiteUrl[] = "https://example.test";
constexpr char kSubdomainUrl[] = "https://sub.example.test";
constexpr char kOtherSiteUrl[] = "https://other.test";

url::Origin OriginFor(std::string_view url) {
  return url::Origin::Create(GURL(url));
}

net::IsolationInfo TopLevelIsolationInfo(const url::Origin& origin) {
  return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                    origin, origin,
                                    net::SiteForCookies::FromOrigin(origin));
}

std::vector<std::string> SessionIds(
    base::span<const mojom::RestrictedDeviceBoundSessionPtr> sessions) {
  return base::ToVector(sessions,
                        &mojom::RestrictedDeviceBoundSession::session_id);
}

class TestObserver : public mojom::RestrictedDeviceBoundSessionObserver {
 public:
  mojo::PendingAssociatedRemote<mojom::RestrictedDeviceBoundSessionObserver>
  BindNewEndpointAndPassRemote() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  // Waits until every push the manager sent before this call has arrived.
  void Flush() { receiver_.FlushForTesting(); }

  std::vector<std::string> pushed_ids() const { return SessionIds(pushed_); }

  // mojom::RestrictedDeviceBoundSessionObserver:
  void OnDeviceBoundSessionRegistered(
      mojom::RestrictedDeviceBoundSessionPtr session) override {
    pushed_.push_back(std::move(session));
  }

 private:
  mojo::AssociatedReceiver<mojom::RestrictedDeviceBoundSessionObserver>
      receiver_{this};
  std::vector<mojom::RestrictedDeviceBoundSessionPtr> pushed_;
};

class RestrictedDeviceBoundSessionManagerImplTest : public testing::Test {
 public:
  RestrictedDeviceBoundSessionManagerImplTest() {
    SetSiteCookieSetting(CONTENT_SETTING_ALLOW);

    ON_CALL(session_service_, AddEventObserver(_))
        .WillByDefault([this](SessionServiceMock::OnEventCallback callback) {
          return event_callbacks_.Add(std::move(callback));
        });
    ON_CALL(session_service_, GetSession(_))
        .WillByDefault([this](const SessionKey& key) -> const Session* {
          auto it = sessions_.find(key);
          return it == sessions_.end() ? nullptr : it->second.get();
        });
    // Like SessionServiceImpl once loaded: copy the keys now, reply later.
    ON_CALL(session_service_, GetAllSessionsAsync(_))
        .WillByDefault([this](SessionKeysCallback callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), StoredKeys()));
        });
  }

 protected:
  // Installs a site-level COOKIES setting for example.test, the setting that
  // also governs localStorage. Other sites keep the default ALLOW.
  void SetSiteCookieSetting(ContentSetting setting) {
    cookie_settings_.set_content_settings(
        ContentSettingsType::COOKIES,
        {ContentSettingPatternSource(
             ContentSettingsPattern::FromString("[*.]example.test"),
             ContentSettingsPattern::Wildcard(), base::Value(setting),
             content_settings::ProviderType::kNone, /*incognito=*/false),
         ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                     ContentSettingsPattern::Wildcard(),
                                     base::Value(CONTENT_SETTING_ALLOW),
                                     content_settings::ProviderType::kNone,
                                     /*incognito=*/false)});
  }

  // Stores a session the way a successful registration from `fetcher_url`
  // would, and returns its key.
  SessionKey StoreSession(const std::string& session_id,
                          std::string_view fetcher_url,
                          std::string_view scope_origin,
                          bool include_site) {
    SessionParams params = {
        .session_id = session_id,
        .fetcher_url = GURL(fetcher_url),
        .refresh_url = "/refresh",
        .scope = {.origin = std::string(scope_origin)},
        .credentials = {{
            .name = "auth",
            .attributes = "Secure; Domain=example.test",
        }},
        .key_id = unexportable_keys::UnexportableSigningKeyId(),
        .allowed_refresh_initiators = {"*"},
    };
    params.scope.include_site = include_site;
    base::expected<std::unique_ptr<Session>, SessionError> session_or_error =
        Session::CreateIfValid(params);
    CHECK(session_or_error.has_value());
    SessionKey key{net::SchemefulSite(GURL(fetcher_url)),
                   Session::Id(session_id)};
    sessions_[key] = std::move(*session_or_error);
    return key;
  }

  // Stores a site-scoped session registered on https://example.test.
  SessionKey StoreSiteSession(const std::string& session_id) {
    return StoreSession(session_id, "https://example.test/login", kSiteUrl,
                        /*include_site=*/true);
  }

  std::vector<SessionKey> StoredKeys() const {
    return base::ToVector(sessions_,
                          [](const auto& entry) { return entry.first; });
  }

  void FireCreation(const SessionKey& key, bool succeeded = true) {
    event_callbacks_.Notify(SessionEvent::MakeCreationEvent(
        key.site, key.id.value(), succeeded,
        SessionError{succeeded ? SessionError::kSuccess
                               : SessionError::kInvalidFetcherUrl},
        /*new_session_display=*/std::nullopt));
  }

  void FireTermination(const SessionKey& key) {
    event_callbacks_.Notify(SessionEvent::MakeTerminationEvent(
        key.site, key.id.value(), /*succeeded=*/true,
        DeletionReason::kExpired));
  }

  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> CreateManager(
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info) {
    mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> remote;
    managers_.Add(
        std::make_unique<RestrictedDeviceBoundSessionManagerImpl>(
            session_service_, cookie_settings_, origin, isolation_info),
        remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  // A manager for a top-level https://sub.example.test Document.
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager>
  CreateSubdomainManager() {
    url::Origin origin = OriginFor(kSubdomainUrl);
    return CreateManager(origin, TopLevelIsolationInfo(origin));
  }

  SessionsFuture Subscribe(
      mojo::Remote<mojom::RestrictedDeviceBoundSessionManager>& manager,
      TestObserver& observer) {
    SessionsFuture future;
    manager->SubscribeToRegistrations(observer.BindNewEndpointAndPassRemote(),
                                      future.GetCallback());
    return future;
  }

  base::test::TaskEnvironment task_environment_;
  CookieSettings cookie_settings_;
  std::map<SessionKey, std::unique_ptr<Session>> sessions_;
  base::RepeatingCallbackList<void(const SessionEvent&)> event_callbacks_;
  NiceMock<SessionServiceMock> session_service_;
  // Declared last so the managers die before the service, settings and
  // callback list they point to.
  mojo::UniqueReceiverSet<mojom::RestrictedDeviceBoundSessionManager> managers_;
};

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       IncludeSiteSessionVisibleOnSubdomain) {
  StoreSiteSession("existing");
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;

  EXPECT_THAT(SessionIds(Subscribe(manager, observer).Take()),
              ElementsAre("existing"));

  FireCreation(StoreSiteSession("created"));
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), ElementsAre("created"));
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       OriginScopedSessionHiddenFromSiblingOrigin) {
  StoreSession("existing", "https://a.example.test/login",
               "https://a.example.test", /*include_site=*/false);
  url::Origin origin = OriginFor("https://b.example.test");
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateManager(origin, TopLevelIsolationInfo(origin));
  TestObserver observer;

  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());

  FireCreation(StoreSession("created", "https://a.example.test/login",
                            "https://a.example.test",
                            /*include_site=*/false));
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), IsEmpty());
}

// A same-site embedded frame is a first-party context, so blocking
// third-party cookies does not hide sessions from it.
TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       SameSiteFrameSeesSessionsWithThirdPartyCookiesBlocked) {
  cookie_settings_.set_block_third_party_cookies(true);
  StoreSiteSession("existing");
  url::Origin top_frame_origin = OriginFor(kSiteUrl);
  url::Origin origin = OriginFor(kSubdomainUrl);
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateManager(
          origin,
          net::IsolationInfo::Create(
              net::IsolationInfo::RequestType::kOther, top_frame_origin, origin,
              net::SiteForCookies::FromOrigin(top_frame_origin)));
  TestObserver observer;

  EXPECT_THAT(SessionIds(Subscribe(manager, observer).Take()),
              ElementsAre("existing"));
  FireCreation(StoreSiteSession("created"));
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), ElementsAre("created"));
}

// Changes the setting between events on the same manager, so it proves the
// check runs on every use.
TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       SiteDataBlockSilencesThenUnblockResumes) {
  StoreSiteSession("before_block");
  SetSiteCookieSetting(CONTENT_SETTING_BLOCK);
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver existing_observer;

  EXPECT_THAT(Subscribe(manager, existing_observer).Take(), IsEmpty());
  FireCreation(StoreSiteSession("while_blocked"));
  existing_observer.Flush();
  EXPECT_THAT(existing_observer.pushed_ids(), IsEmpty());

  SetSiteCookieSetting(CONTENT_SETTING_ALLOW);
  FireCreation(StoreSiteSession("after_unblock"));
  existing_observer.Flush();
  EXPECT_THAT(existing_observer.pushed_ids(), ElementsAre("after_unblock"));

  TestObserver new_observer;
  EXPECT_THAT(SessionIds(Subscribe(manager, new_observer).Take()),
              ElementsAre("after_unblock", "before_block", "while_blocked"));
  FireCreation(StoreSiteSession("later"));
  existing_observer.Flush();
  new_observer.Flush();
  EXPECT_THAT(existing_observer.pushed_ids(),
              ElementsAre("after_unblock", "later"));
  EXPECT_THAT(new_observer.pushed_ids(), ElementsAre("later"));
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       ThreeSubscriptionsOneEventObserver) {
  EXPECT_CALL(session_service_, AddEventObserver(_))
      .WillOnce([this](SessionServiceMock::OnEventCallback callback) {
        return event_callbacks_.Add(std::move(callback));
      });
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observers[3];
  for (TestObserver& observer : observers) {
    EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());
  }

  FireCreation(StoreSiteSession("created"));
  for (TestObserver& observer : observers) {
    observer.Flush();
    EXPECT_THAT(observer.pushed_ids(), ElementsAre("created"));
  }
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       DestroyingPipeCancelsSubscription) {
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());
  EXPECT_FALSE(event_callbacks_.empty());

  manager.reset();
  managers_.FlushForTesting();
  EXPECT_TRUE(event_callbacks_.empty());
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       LastObserverDisconnectReleasesSubscription) {
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  auto first = std::make_unique<TestObserver>();
  auto second = std::make_unique<TestObserver>();
  EXPECT_THAT(Subscribe(manager, *first).Take(), IsEmpty());
  EXPECT_THAT(Subscribe(manager, *second).Take(), IsEmpty());

  first.reset();
  manager.FlushForTesting();
  EXPECT_FALSE(event_callbacks_.empty());

  second.reset();
  manager.FlushForTesting();
  EXPECT_TRUE(event_callbacks_.empty());

  // A later call subscribes again.
  TestObserver third;
  EXPECT_THAT(Subscribe(manager, third).Take(), IsEmpty());
  FireCreation(StoreSiteSession("created"));
  third.Flush();
  EXPECT_THAT(third.pushed_ids(), ElementsAre("created"));
}

// The key is in the reply, and no creation event fires afterwards.
TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       SessionStoredBeforeSubscribeIsInReplyOnly) {
  StoreSiteSession("existing");
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;

  EXPECT_THAT(SessionIds(Subscribe(manager, observer).Take()),
              ElementsAre("existing"));
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), IsEmpty());
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       JoinsObserversBeforeGetAllSessionsAsync) {
  InSequence sequence;
  EXPECT_CALL(session_service_, AddEventObserver(_))
      .WillOnce([this](SessionServiceMock::OnEventCallback callback) {
        return event_callbacks_.Add(std::move(callback));
      });
  EXPECT_CALL(session_service_, GetAllSessionsAsync(_))
      .WillOnce([this](SessionKeysCallback callback) {
        // The event fires before the reply is produced, so it can only
        // reach the remote if the remote already joined.
        FireCreation(StoreSiteSession("created_before_reply"));
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::vector<SessionKey>()));
      });
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;

  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), ElementsAre("created_before_reply"));
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       SessionCreatedAfterSubscribeIsPushedExactlyOnce) {
  base::test::TestFuture<SessionKeysCallback> get_all_sessions_call;
  EXPECT_CALL(session_service_, GetAllSessionsAsync(_))
      .WillOnce([&](SessionKeysCallback callback) {
        get_all_sessions_call.SetValue(std::move(callback));
      });
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  SessionsFuture reply = Subscribe(manager, observer);
  SessionKeysCallback reply_with_keys = get_all_sessions_call.Take();

  FireCreation(StoreSiteSession("created"));
  // The keys were copied before the session was stored.
  std::move(reply_with_keys).Run({});

  EXPECT_THAT(reply.Take(), IsEmpty());
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), ElementsAre("created"));
}

// While the store loads, GetAllSessionsAsync() is queued, so a session
// registered meanwhile is pushed and also listed in the later reply. The push
// shares the manager pipe with the reply, so it arrives first, and Blink
// removes the copy.
TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       LoadTimeSessionReachesBothPaths) {
  base::test::TestFuture<SessionKeysCallback> get_all_sessions_call;
  EXPECT_CALL(session_service_, GetAllSessionsAsync(_))
      .WillOnce([&](SessionKeysCallback callback) {
        get_all_sessions_call.SetValue(std::move(callback));
      });
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  base::RunLoop reply_loop;
  manager->SubscribeToRegistrations(
      observer.BindNewEndpointAndPassRemote(),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::RestrictedDeviceBoundSessionPtr> sessions) {
            EXPECT_THAT(SessionIds(sessions), ElementsAre("loaded"));
            // Because pushes and the reply are ordered on the same associated
            // pipe, the push must have already arrived before this reply.
            EXPECT_THAT(observer.pushed_ids(), ElementsAre("loaded"));
            reply_loop.Quit();
          }));
  SessionKeysCallback reply_with_keys = get_all_sessions_call.Take();

  SessionKey key = StoreSiteSession("loaded");
  FireCreation(key);
  std::move(reply_with_keys).Run({key});
  reply_loop.Run();
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       ReRegisteredSessionIsPushedAgain) {
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());

  SessionKey key = StoreSiteSession("reused");
  FireCreation(key);
  FireTermination(key);
  FireCreation(StoreSiteSession("reused"));
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), ElementsAre("reused", "reused"));
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       IgnoresFailedAndNonCreationEvents) {
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());

  SessionKey key = StoreSiteSession("stored");
  FireCreation(key, /*succeeded=*/false);
  FireTermination(key);
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), IsEmpty());
}

TEST_F(RestrictedDeviceBoundSessionManagerImplTest, IgnoresOtherSiteEvents) {
  SessionKey other_key{net::SchemefulSite(GURL(kOtherSiteUrl)),
                       Session::Id("other")};
  sessions_[other_key] = nullptr;
  EXPECT_CALL(session_service_, GetSession(_)).Times(0);

  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;
  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());

  FireCreation(other_key);
  observer.Flush();
  EXPECT_THAT(observer.pushed_ids(), IsEmpty());
}

// A session deleted between the key copy and the reply leaves a key with no
// session behind.
TEST_F(RestrictedDeviceBoundSessionManagerImplTest,
       ReplySkipsKeyWithoutSession) {
  EXPECT_CALL(session_service_, GetAllSessionsAsync(_))
      .WillOnce([](SessionKeysCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      std::vector<SessionKey>{
                                          {net::SchemefulSite(GURL(kSiteUrl)),
                                           Session::Id("deleted")}}));
      });
  mojo::Remote<mojom::RestrictedDeviceBoundSessionManager> manager =
      CreateSubdomainManager();
  TestObserver observer;

  EXPECT_THAT(Subscribe(manager, observer).Take(), IsEmpty());
}

using RestrictedDeviceBoundSessionManagerImplDeathTest =
    RestrictedDeviceBoundSessionManagerImplTest;

TEST_F(RestrictedDeviceBoundSessionManagerImplDeathTest,
       ConstructorChecksFrameOrigin) {
  url::Origin origin = OriginFor(kSubdomainUrl);
  net::IsolationInfo other_frame = TopLevelIsolationInfo(OriginFor(kSiteUrl));
  EXPECT_CHECK_DEATH(RestrictedDeviceBoundSessionManagerImpl(
      session_service_, cookie_settings_, origin, other_frame));
}

TEST_F(RestrictedDeviceBoundSessionManagerImplDeathTest,
       ConstructorChecksNotOpaque) {
  url::Origin opaque_origin;
  net::IsolationInfo opaque_frame = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, opaque_origin, opaque_origin,
      net::SiteForCookies());
  EXPECT_CHECK_DEATH_WITH(
      RestrictedDeviceBoundSessionManagerImpl(
          session_service_, cookie_settings_, opaque_origin, opaque_frame),
      "opaque");
}

TEST_F(RestrictedDeviceBoundSessionManagerImplDeathTest,
       ConstructorChecksPotentiallyTrustworthy) {
  url::Origin origin = OriginFor("http://example.test");
  EXPECT_CHECK_DEATH_WITH(RestrictedDeviceBoundSessionManagerImpl(
                              session_service_, cookie_settings_, origin,
                              TopLevelIsolationInfo(origin)),
                          "IsOriginPotentiallyTrustworthy");
}

TEST_F(RestrictedDeviceBoundSessionManagerImplDeathTest,
       ConstructorChecksNoNonce) {
  url::Origin origin = OriginFor(kSubdomainUrl);
  net::IsolationInfo nonced_frame = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, origin, origin,
      net::SiteForCookies::FromOrigin(origin),
      base::UnguessableToken::Create());
  EXPECT_CHECK_DEATH_WITH(
      RestrictedDeviceBoundSessionManagerImpl(
          session_service_, cookie_settings_, origin, nonced_frame),
      "nonce");
}

TEST_F(RestrictedDeviceBoundSessionManagerImplDeathTest,
       ConstructorChecksFirstParty) {
  url::Origin origin = OriginFor(kSubdomainUrl);
  net::IsolationInfo cross_site_frame = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, OriginFor(kOtherSiteUrl), origin,
      net::SiteForCookies());
  EXPECT_CHECK_DEATH_WITH(
      RestrictedDeviceBoundSessionManagerImpl(
          session_service_, cookie_settings_, origin, cross_site_frame),
      "IsFirstParty");
}

}  // namespace
}  // namespace network
