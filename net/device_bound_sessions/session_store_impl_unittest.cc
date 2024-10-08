// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util_internal.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

unexportable_keys::UnexportableKeyId GenerateNewKey(
    unexportable_keys::UnexportableKeyService& key_service) {
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      generate_future;
  key_service.GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
  unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
      key_id = generate_future.Get();
  CHECK(key_id.has_value());
  return *key_id;
}

std::vector<uint8_t> GetWrappedKey(
    unexportable_keys::UnexportableKeyService& key_service,
    const unexportable_keys::UnexportableKeyId& key_id) {
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      key_service.GetWrappedKey(key_id);
  CHECK(wrapped_key.has_value());
  return *wrapped_key;
}

bool SessionMapsAreEqual(const SessionStore::SessionsMap& lhs,
                         const SessionStore::SessionsMap& rhs) {
  return base::ranges::is_permutation(
      lhs, rhs, [&](const auto& pair1, const auto& pair2) {
        return pair1.first == pair2.first &&
               pair1.second->IsEqualForTesting(*pair2.second);
      });
}

std::unique_ptr<Session> CreateSessionHelper(
    unexportable_keys::UnexportableKeyService& key_service,
    const std::string& url_string,
    const std::string& session_id) {
  SessionParams::Scope scope;
  std::string cookie_attr = "Secure; Domain=" + GURL(url_string).host();
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie", cookie_attr}});
  SessionParams params{session_id, url_string, std::move(scope),
                       std::move(cookie_credentials)};
  std::unique_ptr<Session> session =
      Session::CreateIfValid(params, GURL(url_string));
  session->set_unexportable_key_id(GenerateNewKey(key_service));
  return session;
}

proto::Session CreateSessionProto(
    unexportable_keys::UnexportableKeyService& key_service,
    const std::string& url_string,
    const std::string& session_id) {
  std::unique_ptr<Session> session =
      CreateSessionHelper(key_service, url_string, session_id);
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
        CreateSessionHelper(key_service, cfg.url, cfg.session_id);
    EXPECT_TRUE(session);
    store.SaveSession(site, *session);
    session_map.emplace(std::move(site), std::move(session));
  }

  return session_map;
}

}  // namespace

class SessionStoreImplTest : public testing::Test {
 public:
  SessionStoreImplTest()
      : unexportable_key_service_(unexportable_key_task_manager_) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
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
        site, session->id(),
        base::BindLambdaForTesting(
            [&run_loop,
             &session](unexportable_keys::ServiceErrorOr<
                       unexportable_keys::UnexportableKeyId> key_id_or_error) {
              session->set_unexportable_key_id(key_id_or_error);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  std::unique_ptr<SessionStoreImpl> store_;
};

TEST_F(SessionStoreImplTest, FailDBLoadFromInvalidPath) {
  base::FilePath invalid_path(FILE_PATH_LITERAL("o:\\inaccessible-path"));
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
  store().DeleteSession(site, session->id());
  EXPECT_EQ(store().GetAllSessions().size(), 0u);

  // Verify that restore session binding key call fails.
  RestoreSessionBindingKey(site, session.get());
  EXPECT_TRUE(session->unexportable_key_id() ==
              base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
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
      {"https://a.foo.test/index.html", "session0"},  // schemeful site 1
      {"https://b.foo.test/index.html", "session1"},  // ""
      {"https://c.bar.test/index.html", "session2"},  // schemeful site 2
  };
  SessionStore::SessionsMap expected_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  // Retrieve all sessions from the store.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();

  // Restore the binding keys in the store session objects.
  for (auto& [site, session] : store_sessions) {
    RestoreSessionBindingKey(site, session.get());
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

  // Modify the existing session and save it again to the store.
  session->set_expiry_date(base::Time::Now());
  store().SaveSession(site, *session);

  // Retrieve the session from the store and check that its contents
  // match the updated data.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 1u);
  for (auto& [store_site, store_session] : store_sessions) {
    EXPECT_TRUE(store_site == site);
    EXPECT_TRUE(store_session->expiry_date() == session->expiry_date());
    RestoreSessionBindingKey(store_site, store_session.get());
    EXPECT_TRUE(store_session->IsEqualForTesting(*session));
  }
}

TEST_F(SessionStoreImplTest, HandleNonexistingSite) {
  CreateStoreAndLoadSessions();

  // Try to delete a session associated with a nonexisting site (in the store).
  auto site = net::SchemefulSite(GURL("https://foo.test"));
  store().DeleteSession(site, Session::Id("session"));
  EXPECT_EQ(store().GetAllSessions().size(), 0u);

  // Create a session but don't save it to the store.
  std::unique_ptr<Session> session = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session");
  // Try to restore that session's binding key. Note that the store doesn't have
  // an entry for the associated site.
  RestoreSessionBindingKey(site, session.get());
  EXPECT_EQ(store().GetAllSessions().size(), 0u);
  EXPECT_TRUE(session->unexportable_key_id() ==
              base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
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
  store().DeleteSession(site, session2->id());
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Try to restore the unsaved session's binding key.
  RestoreSessionBindingKey(site, session2.get());
  EXPECT_EQ(store().GetAllSessions().size(), 1u);
  EXPECT_TRUE(session2->unexportable_key_id() ==
              base::unexpected(unexportable_keys::ServiceError::kKeyNotFound));
}

TEST_F(SessionStoreImplTest, DeleteSessions) {
  CreateStoreAndLoadSessions();

  // Create and save some sessions.
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0"},  // schemeful site 1
      {"https://b.foo.test/index.html", "session1"},  // ""
      {"https://c.bar.test/index.html", "session2"},  // schemeful site 2
  };
  SessionStore::SessionsMap expected_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  auto site1 = net::SchemefulSite(GURL(cfgs[0].url));
  auto site2 = net::SchemefulSite(GURL(cfgs[2].url));

  // Retrieve all sessions from the store.
  SessionStore::SessionsMap store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 3u);

  // Delete the valid sessions one by one and check store contents.
  store().DeleteSession(site2, Session::Id(cfgs[2].session_id));
  store_sessions = store().GetAllSessions();
  EXPECT_TRUE(store_sessions.find(site2) == store_sessions.end());

  store().DeleteSession(site1, Session::Id(cfgs[0].session_id));
  store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 1u);
  EXPECT_EQ(store_sessions.begin()->first, site1);
  EXPECT_EQ(store_sessions.begin()->second->id(),
            Session::Id(cfgs[1].session_id));

  store().DeleteSession(site1, Session::Id(cfgs[1].session_id));
  store_sessions = store().GetAllSessions();
  EXPECT_EQ(store_sessions.size(), 0u);
}

TEST_F(SessionStoreImplTest, LoadSavedSessions) {
  CreateStoreAndLoadSessions();
  SessionCfgList cfgs = {
      {"https://a.foo.test/index.html", "session0"},
      {"https://b.foo.test/index.html", "session1"},
      {"https://c.bar.test/index.html", "session2"},
  };

  SessionStore::SessionsMap saved_sessions =
      CreateAndSaveSessions(cfgs, unexportable_key_service(), store());

  MimicRestart();

  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  // Restore the binding keys in the store session objects.
  for (auto& [site, session] : loaded_sessions) {
    RestoreSessionBindingKey(site, session.get());
  }

  EXPECT_TRUE(SessionMapsAreEqual(saved_sessions, loaded_sessions));
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidSite) {
  // Create an entry with an invalid site.
  proto::Session sproto = CreateSessionProto(unexportable_key_service(),
                                             "https://foo.test", "session_id");
  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_id"] = std::move(sproto);

  // Create an entry with a valid site.
  proto::Session sproto2 = CreateSessionProto(unexportable_key_service(),
                                              "https://bar.test", "session_id");
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
  EXPECT_EQ(sessions_map.count(site2), 1u);
  EXPECT_EQ(keys_to_delete.size(), 1u);
  EXPECT_EQ(keys_to_delete[0], "about:blank");
}

// Note: There are several reasons why a session may be invalid. We only
// use one of them here to test the pruning logic. The individual invalid
// reasons have been tested in SessionTest.FailCreateFromInvalidProto
// in file session_unittest.cc
TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidSession) {
  // Create an entry with 1 valid and 1 invalid session.
  proto::Session sproto1 = CreateSessionProto(
      unexportable_key_service(), "https://foo.example.test", "session_1");
  // Create an invalid session.
  proto::Session sproto2 = CreateSessionProto(
      unexportable_key_service(), "https://bar.example.test", "session_2");
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
  proto::Session sproto = CreateSessionProto(
      unexportable_key_service(), "https://foo.example.test", "session_id");
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

}  // namespace net::device_bound_sessions
