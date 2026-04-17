// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store_impl.h"

#include <memory>
#include <string_view>

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

using ::base::test::ErrorIs;
using ::testing::ElementsAre;
using ::testing::Return;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;
constexpr unexportable_keys::BackgroundTaskOrigin kTaskOrigin =
    unexportable_keys::BackgroundTaskOrigin::kDeviceBoundSessionCredentials;
constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);

unexportable_keys::UnexportableSigningKeyId GenerateNewSigningKey(
    unexportable_keys::UnexportableKeyService& key_service) {
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      generate_future;
  key_service.GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
  unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableSigningKeyId>
      key_id = generate_future.Get();
  CHECK(key_id.has_value());
  return *key_id;
}

std::vector<uint8_t> GetWrappedKey(
    unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableKeyId key_id) {
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      key_service.GetWrappedKey(key_id);
  CHECK(wrapped_key.has_value());
  return *wrapped_key;
}

bool SessionMapsAreEqual(const SessionStore::SessionsMap& lhs,
                         const SessionStore::SessionsMap& rhs) {
  return std::ranges::is_permutation(
      lhs, rhs, [&](const auto& pair1, const auto& pair2) {
        return pair1.first == pair2.first &&
               pair1.second->IsEqualForTesting(*pair2.second);
      });
}

std::unique_ptr<Session> CreateSessionHelper(
    unexportable_keys::UnexportableKeyId key_id,
    std::string_view url_string,
    std::string_view session_id,
    std::string_view origin = "https://foo.test") {
  SessionParams::Scope scope;
  scope.origin = origin;
  std::string cookie_attr = "Secure; Domain=" + GURL(url_string).GetHost();
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie", cookie_attr}});
  SessionParams params{std::string(session_id),          GURL(url_string),
                       std::string(url_string),          std::move(scope),
                       std::move(cookie_credentials),    key_id,
                       /*allowed_refresh_initiators=*/{}};
  return *Session::CreateIfValid(params);
}

std::unique_ptr<Session> CreateSessionHelper(
    unexportable_keys::UnexportableKeyService& key_service,
    std::string_view url_string,
    std::string_view session_id,
    std::string_view origin = "https://foo.test") {
  return CreateSessionHelper(GenerateNewSigningKey(key_service), url_string,
                             session_id, origin);
}

proto::Session CreateSessionProto(
    unexportable_keys::UnexportableKeyService& key_service,
    const std::string& url_string,
    const std::string& session_id,
    const std::string& origin) {
  std::unique_ptr<Session> session =
      CreateSessionHelper(key_service, url_string, session_id, origin);
  proto::Session sproto = session->ToProto();
  unexportable_keys::UnexportableKeyId key_id =
      session->unexportable_key_id().value();
  std::vector<uint8_t> wrapped_key = GetWrappedKey(key_service, key_id);
  sproto.set_wrapped_key(std::string(wrapped_key.begin(), wrapped_key.end()));
  return sproto;
}

struct SessionCfg {
  std::string url;
  std::string session_id;
  std::string origin;
};
using SessionCfgList = std::vector<SessionCfg>;
SessionStore::SessionsMap CreateAndSaveSessions(
    const SessionCfgList& cfgs,
    unexportable_keys::UnexportableKeyService& key_service,
    SessionStore& store) {
  SessionStore::SessionsMap session_map;
  for (auto& cfg : cfgs) {
    auto site = net::SchemefulSite(GURL(cfg.url));
    std::unique_ptr<Session> session =
        CreateSessionHelper(key_service, cfg.url, cfg.session_id, cfg.origin);
    EXPECT_TRUE(session);
    store.SaveSession(site, *session);
    session_map.emplace(SessionKey{std::move(site), session->id()},
                        std::move(session));
  }

  return session_map;
}

}  // namespace

class SessionStoreImplTest : public net::TestWithTaskEnvironment {
 public:
  SessionStoreImplTest()
      : net::TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  ~SessionStoreImplTest() override = default;

  void TearDown() override {
    if (store_) {
      DeleteStore();
    }
  }

  SessionStoreImpl& store() { return *store_; }

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  base::FilePath GetDBPath() const {
    return temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("db_file")));
  }

  void CreateStore(base::FilePath db_path) {
    store_ =
        std::make_unique<SessionStoreImpl>(db_path, unexportable_key_service_);
  }

  void DeleteStore() {
    base::RunLoop run_loop;
    store_->SetShutdownCallbackForTesting(run_loop.QuitClosure());
    store_ = nullptr;
    run_loop.Run();
  }

  void MimicRestart() {
    DeleteStore();
    CreateStore(GetDBPath());
  }

  SessionStore::SessionsMap LoadSessions() {
    base::RunLoop run_loop;
    SessionStore::SessionsMap loaded_sessions;
    store_->LoadSessions(base::BindLambdaForTesting(
        [&run_loop, &loaded_sessions](SessionStore::SessionsMap sessions) {
          loaded_sessions = std::move(sessions);
          run_loop.Quit();
        }));
    run_loop.Run();
    return loaded_sessions;
  }

  void CreateStoreAndLoadSessions() {
    CreateStore(GetDBPath());
    SessionStore::SessionsMap sessions = LoadSessions();
    EXPECT_TRUE(store().db_status() == SessionStoreImpl::DBStatus::kSuccess);
    EXPECT_EQ(sessions.size(), 0u);
  }

  void RestoreSessionBindingKey(const SchemefulSite& site, Session* session) {
    base::RunLoop run_loop;
    store_->RestoreSessionBindingKey(
        SessionKey{site, session->id()},
        base::BindLambdaForTesting(
            [&run_loop,
             &session](unexportable_keys::ServiceErrorOr<
                       unexportable_keys::UnexportableKeyId> key_id_or_error) {
              session->set_unexportable_key_id(key_id_or_error);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  unexportable_keys::ScopedMockUnexportableKeyProvider&
  SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<unexportable_keys::ScopedMockUnexportableKeyProvider>();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  base::ScopedTempDir temp_dir_;
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               unexportable_keys::ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      unexportable_key_task_manager_, kTaskOrigin,
      crypto::UnexportableKeyProvider::Config()};
  std::unique_ptr<SessionStoreImpl> store_;
};

TEST_F(SessionStoreImplTest, FailDBLoadFromInvalidPath) {
  base::FilePath invalid_path(FILE_PATH_LITERAL("o://inaccessible-path"));
  CreateStore(invalid_path);
  LoadSessions();
  EXPECT_FALSE(store().db_status() == SessionStoreImpl::DBStatus::kSuccess);
}

TEST_F(SessionStoreImplTest, InitializeStore) {
  CreateStoreAndLoadSessions();
}

TEST_F(SessionStoreImplTest, RequireDBInit) {
  // Create a store but don't initialize DB with an initial load.
  CreateStore(GetDBPath());
  EXPECT_TRUE(store().db_status() != SessionStoreImpl::DBStatus::kSuccess);

  // Verify that save session call is ignored.
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session1");
  auto site = net::SchemefulSite(GURL("https://foo.test"));
  store().SaveSession(site, *session);
  EXPECT_EQ(store().GetAllSessions().size(), 0u);

  // Verify that delete session call is ignored.
  store().DeleteSession(SessionKey{site, session->id()});
  EXPECT_EQ(store().GetAllSessions().size(), 0u);

  // Verify that restore session binding key call fails.
  RestoreSessionBindingKey(site, session.get());
  EXPECT_THAT(session->unexportable_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
}

TEST_F(SessionStoreImplTest, RequireValidBindingKeyForSave) {
  CreateStoreAndLoadSessions();
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session1");
  session->set_unexportable_key_id(unexportable_keys::UnexportableKeyId());
  store().SaveSession(net::SchemefulSite(GURL("https://foo.test")), *session);
  EXPECT_EQ(store().GetAllSessions().size(), 0u);
}

TEST_F(SessionStoreImplTest, SaveNewSessions) {
  CreateStoreAndLoadSessions();
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0",
       "https://foo.test"},  // schemeful site 1
      {"https://b.foo.test/index.html", "session1",
       "https://foo.test"},  // schemeful site 1
      {"https://c.bar.test/index.html", "session2",
       "https://bar.test"},  // schemeful site 2
  };
  SessionStore::SessionsMap expected_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  // Retrieve all sessions from the store.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();

  // Restore the binding keys in the store session objects.
  for (auto& [key, session] : store_sessions) {
    RestoreSessionBindingKey(key.site, session.get());
  }

  // Verify the session store contents.
  EXPECT_TRUE(SessionMapsAreEqual(expected_sessions, store_sessions));
}

TEST_F(SessionStoreImplTest, UpdateExistingSession) {
  CreateStoreAndLoadSessions();

  // Save a new session.
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session1");
  auto site = net::SchemefulSite(GURL("https://foo.test"));
  store().SaveSession(site, *session);
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Modify the existing session and save it again to the store. The
  // save will fail if time advances past the expiry date, so use a 10
  // second margin of safety. This is arbitrary, as long as it's longer
  // than it takes to save a session.
  session->set_expiry_date(base::Time::Now() + base::Seconds(10));
  store().SaveSession(site, *session);

  // Retrieve the session from the store and check that its contents
  // match the updated data.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 1u);
  for (auto& [key, store_session] : store_sessions) {
    EXPECT_TRUE(key.site == site);
    EXPECT_TRUE(key.id == store_session->id());
    EXPECT_TRUE(store_session->expiry_date() == session->expiry_date());
    RestoreSessionBindingKey(key.site, store_session.get());
    EXPECT_TRUE(store_session->IsEqualForTesting(*session));
  }
}

TEST_F(SessionStoreImplTest, HandleNonexistingSite) {
  CreateStoreAndLoadSessions();

  // Try to delete a session associated with a nonexisting site (in the store).
  auto site = net::SchemefulSite(GURL("https://foo.test"));
  store().DeleteSession(SessionKey{site, Session::Id("session")});
  EXPECT_EQ(store().GetAllSessions().size(), 0u);

  // Create a session but don't save it to the store.
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session");
  // Try to restore that session's binding key. Note that the store doesn't have
  // an entry for the associated site.
  RestoreSessionBindingKey(site, session.get());
  EXPECT_EQ(store().GetAllSessions().size(), 0u);
  EXPECT_THAT(session->unexportable_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
}

TEST_F(SessionStoreImplTest, HandleNonexistingSession) {
  CreateStoreAndLoadSessions();

  // Save a session.
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session1");
  auto site = net::SchemefulSite(GURL("https://foo.test"));
  store().SaveSession(site, *session);
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Create another but don't save it to the store.
  std::unique_ptr<Session> session2 = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session2");

  // Try to delete the unsaved session.
  store().DeleteSession(SessionKey{site, session2->id()});
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Try to restore the unsaved session's binding key.
  RestoreSessionBindingKey(site, session2.get());
  EXPECT_EQ(store().GetAllSessions().size(), 1u);
  EXPECT_THAT(session2->unexportable_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
}

TEST_F(SessionStoreImplTest, DeleteSessions) {
  CreateStoreAndLoadSessions();

  // Create and save some sessions.
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0",
       "https://foo.test"},  // schemeful site 1
      {"https://b.foo.test/index.html", "session1",
       "https://foo.test"},  // schemeful site 1
      {"https://c.bar.test/index.html", "session2",
       "https://bar.test"},  // schemeful site 2
  };
  SessionStore::SessionsMap expected_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  auto site1 = net::SchemefulSite(GURL(cfgs[0].url));
  auto site2 = net::SchemefulSite(GURL(cfgs[2].url));

  // Retrieve all sessions from the store.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 3u);

  // Delete the valid sessions one by one and check store contents.
  store().DeleteSession(SessionKey{site2, Session::Id(cfgs[2].session_id)});
  store_sessions = store().GetAllSessions();
  EXPECT_TRUE(
      store_sessions.find(SessionKey{site2, Session::Id(cfgs[2].session_id)}) ==
      store_sessions.end());

  store().DeleteSession(SessionKey{site1, Session::Id(cfgs[0].session_id)});
  store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 1u);
  SessionKey expected_key{site1, Session::Id(cfgs[1].session_id)};
  EXPECT_EQ(store_sessions.begin()->first, expected_key);
  EXPECT_EQ(store_sessions.begin()->second->id(),
            Session::Id(cfgs[1].session_id));

  store().DeleteSession(SessionKey{site1, Session::Id(cfgs[1].session_id)});
  store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 0u);
}

TEST_F(SessionStoreImplTest, LoadSavedSessions) {
  CreateStoreAndLoadSessions();
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0", "https://foo.test"},
      {"https://b.foo.test/index.html", "session1", "https://foo.test"},
      {"https://c.bar.test/index.html", "session2", "https://bar.test"},
  };

  SessionStore::SessionsMap saved_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  MimicRestart();

  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  EXPECT_FALSE(loaded_sessions.empty());
  // Restore the binding keys in the store session objects.
  for (auto& [key, session] : loaded_sessions) {
    RestoreSessionBindingKey(key.site, session.get());
  }

  EXPECT_TRUE(SessionMapsAreEqual(saved_sessions, loaded_sessions));
}

TEST_F(SessionStoreImplTest, DropLowerSchemaVersionSessions) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kDeviceBoundSessions,
      {{features::kDeviceBoundSessionsSchemaVersion.name, "1"}});
  CreateStoreAndLoadSessions();
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0", "https://foo.test"},
      {"https://b.foo.test/index.html", "session1", "https://foo.test"},
      {"https://c.bar.test/index.html", "session2", "https://bar.test"},
  };

  SessionStore::SessionsMap saved_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kDeviceBoundSessions,
      {{features::kDeviceBoundSessionsSchemaVersion.name, "2"}});
  MimicRestart();

  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  EXPECT_TRUE(loaded_sessions.empty());
}

TEST_F(SessionStoreImplTest, DropHigherSchemaVersionSessions) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kDeviceBoundSessions,
      {{features::kDeviceBoundSessionsSchemaVersion.name, "2"}});
  CreateStoreAndLoadSessions();
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0", "https://foo.test"},
      {"https://b.foo.test/index.html", "session1", "https://foo.test"},
      {"https://c.bar.test/index.html", "session2", "https://bar.test"},
  };

  SessionStore::SessionsMap saved_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kDeviceBoundSessions,
      {{features::kDeviceBoundSessionsSchemaVersion.name, "1"}});
  MimicRestart();

  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  EXPECT_TRUE(loaded_sessions.empty());
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidSite) {
  // Create an entry with an invalid site.
  proto::Session sproto =
      CreateSessionProto(unexportable_key_service(), "https://foo.test",
                         "session_id", "https://foo.test");
  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_id"] = std::move(sproto);

  // Create an entry with a valid site.
  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://bar.test",
                         "session_id", "https://bar.test");
  proto::SiteSessions site2_proto;
  (*site2_proto.mutable_sessions())["session_id"] = std::move(sproto2);
  auto site2 = net::SchemefulSite(GURL("https://bar.test)"));

  // Create a table with these two entries.
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  loaded_tbl["about:blank"] = std::move(site_proto);
  loaded_tbl[site2.Serialize()] = std::move(site2_proto);

  // Run the 2-entry table through the store's cleaning method.
  std::vector<std::string> keys_to_delete;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(loaded_tbl,
                                                     keys_to_delete);

  // Verify:
  // - entry with valid site is present in the output sessions map.
  // - entry with invalid site is not present and is included in the
  //   keys_to_delete list.
  EXPECT_EQ(sessions_map.size(), 1u);
  SessionKey key{site2, Session::Id("session_id")};
  EXPECT_EQ(sessions_map.count(key), 1u);
  EXPECT_EQ(keys_to_delete.size(), 1u);
  EXPECT_EQ(keys_to_delete[0], "about:blank");
}

// Note: There are several reasons why a session may be invalid. We only
// use one of them here to test the pruning logic. The individual invalid
// reasons have been tested in SessionTest.FailCreateFromInvalidProto
// in file session_unittest.cc
TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidSession) {
  // Create an entry with 1 valid and 1 invalid session.
  proto::Session sproto1 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  // Create an invalid session.
  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://bar.example.test",
                         "session_2", "https://bar.example.test");
  sproto2.set_refresh_url("invalid_url");

  // Create a site proto (proto table's value field) consisting of the above 2
  // sessions.
  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_1"] = std::move(sproto1);
  (*site_proto.mutable_sessions())["session_2"] = std::move(sproto2);

  // Create a table consisting of the above 2-session entry.
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  // Run the DB table through the store's cleaning method.
  std::vector<std::string> keys_to_delete;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(loaded_tbl,
                                                     keys_to_delete);

  // Verify that the entry is pruned even though only 1 out of the 2 sessions
  // was invalid.
  EXPECT_EQ(sessions_map.size(), 0u);
  EXPECT_EQ(keys_to_delete.size(), 1u);
  EXPECT_EQ(keys_to_delete[0], site.Serialize());
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithSessionMissingWrappedKey) {
  // Create a Session proto with missing wrapped key field.
  proto::Session sproto =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_id", "https://foo.example.test");
  sproto.clear_wrapped_key();

  // Create a single entry table with the above session data.
  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_id"] = std::move(sproto);
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  // Run the table through the store's cleaning method.
  std::vector<std::string> keys_to_delete;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(loaded_tbl,
                                                     keys_to_delete);

  // Verify that the DB entry has been pruned in the output sessions map.
  EXPECT_EQ(sessions_map.size(), 0u);
  EXPECT_EQ(keys_to_delete.size(), 1u);
  EXPECT_EQ(keys_to_delete[0], site.Serialize());
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidRefreshInitiator) {
  // Create an entry with an invalid refresh initiator.
  proto::Session sproto =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  sproto.add_allowed_refresh_initiators("a.*.example.test");

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_id"] = std::move(sproto);
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  // Run the table through the store's cleaning method.
  std::vector<std::string> keys_to_delete;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(loaded_tbl,
                                                     keys_to_delete);

  // Verify that the DB entry has been pruned in the output sessions map.
  EXPECT_EQ(sessions_map.size(), 0u);
  EXPECT_EQ(keys_to_delete.size(), 1u);
  EXPECT_EQ(keys_to_delete[0], site.Serialize());
}

TEST_F(SessionStoreImplTest, GarbageCollectsStaleKeys) {
  base::HistogramTester histograms;
  feature_list_.InitAndEnableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  unexportable_keys::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  CreateStoreAndLoadSessions();

  // The first two keys are known to the service, but the third key is stale.
  const std::vector<uint8_t> kWrappedKey1 = {1, 2, 3};
  const std::vector<uint8_t> kWrappedKey2 = {4, 5, 6};
  const std::vector<uint8_t> kStaleWrappedKey = {7, 8, 9};

  EXPECT_CALL(mock_key_provider, GetAllSigningKeysSlowly).WillRepeatedly([=] {
    auto key1 = std::make_unique<unexportable_keys::MockUnexportableKey>();
    auto key2 = std::make_unique<unexportable_keys::MockUnexportableKey>();
    auto stale_key = std::make_unique<unexportable_keys::MockUnexportableKey>();

    ON_CALL(*key1, GetWrappedKey).WillByDefault(Return(kWrappedKey1));
    ON_CALL(*key2, GetWrappedKey).WillByDefault(Return(kWrappedKey2));
    ON_CALL(*stale_key, GetWrappedKey).WillByDefault(Return(kStaleWrappedKey));

    return base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
        std::move(key1),
        std::move(key2),
        std::move(stale_key),
    });
  });

  // Obtain the corresponding key ids.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      std::vector<unexportable_keys::UnexportableKeyId>>>
      get_all_keys_future;
  unexportable_key_service().GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      get_all_keys_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(
      std::vector<unexportable_keys::UnexportableKeyId> all_keys_ids,
      get_all_keys_future.Take());

  unexportable_keys::UnexportableKeyId key_id_1 = all_keys_ids[0];
  unexportable_keys::UnexportableKeyId key_id_2 = all_keys_ids[1];

  // Save two new sessions.
  static constexpr std::string_view kFooSite = "https://foo.test";
  static constexpr std::string_view kBarSite = "https://bar.test";
  store().SaveSession(
      net::SchemefulSite(GURL(kFooSite)),
      *CreateSessionHelper(key_id_1, kFooSite, "session1", kFooSite));
  ASSERT_EQ(store().GetAllSessions().size(), 1u);

  store().SaveSession(
      net::SchemefulSite(GURL(kBarSite)),
      *CreateSessionHelper(key_id_2, kBarSite, "session2", kBarSite));
  ASSERT_EQ(store().GetAllSessions().size(), 2u);

  // Finish loading the sessions, and wait for the stale key to be deleted.
  EXPECT_CALL(mock_key_provider, DeleteSigningKeysSlowly)
      .WillOnce([&](auto keys) {
        auto wrapped_keys = base::ToVector(
            keys, [](auto* key) { return key->GetWrappedKey(); });
        EXPECT_THAT(wrapped_keys, ElementsAre(kStaleWrappedKey));
        return wrapped_keys.size();
      });

  // Advance time to allow StartGarbageCollection to run.
  FastForwardBy(kGarbageCollectionDelay);

  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "TotalKeyCount",
      3, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "UsedKeyCount",
      2, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyCount",
      1, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyDeletionCount",
      1, 1);
}

TEST_F(SessionStoreImplTest, GarbageCollectionDoesNotTriggerIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  unexportable_keys::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  EXPECT_CALL(mock_key_provider, GetAllSigningKeysSlowly).Times(0);
  EXPECT_CALL(mock_key_provider, DeleteAllSigningKeysSlowly).Times(0);

  CreateStoreAndLoadSessions();
}

}  // namespace net::device_bound_sessions
