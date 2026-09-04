// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store_impl.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;

using AttestationKeySaveOutcome = SessionStoreImpl::AttestationKeySaveOutcome;

constexpr crypto::sign::SignatureKind kAcceptableAlgorithms[] = {
    crypto::sign::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;
constexpr unexportable_keys::BackgroundTaskOrigin kTaskOrigin =
    unexportable_keys::BackgroundTaskOrigin::kDeviceBoundSessionCredentials;
constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);
constexpr std::string_view kSite = "https://foo.test";
constexpr char kSessionId[] = "session1";

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

unexportable_keys::UnexportableAttestationKeyId GenerateNewAttestationKey(
    unexportable_keys::UnexportableKeyService& key_service) {
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableAttestationKeyId>>
      generate_future;
  key_service.GenerateAttestationKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
  unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableAttestationKeyId>
      key_id = generate_future.Get();
  CHECK(key_id.has_value());
  return *key_id;
}

std::vector<uint8_t> GetWrappedKey(
    unexportable_keys::UnexportableKeyService& key_service,
    unexportable_keys::UnexportableSigningKeyId key_id) {
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
    unexportable_keys::UnexportableSigningKeyId key_id,
    std::string_view url_string,
    std::string_view session_id = kSessionId,
    std::string_view origin = kSite,
    std::optional<unexportable_keys::UnexportableAttestationKeyId>
        attestation_key_id = std::nullopt) {
  return *Session::CreateIfValid({
      .session_id{session_id},
      .fetcher_url{url_string},
      .refresh_url{url_string},
      .scope =
          {
              // Cross-origin mock sessions must set include_site to true to
              // pass validation.
              .include_site = url::Origin::Create(GURL(url_string)) !=
                              url::Origin::Create(GURL(origin)),
              .origin{origin},
          },
      .credentials = {{
          .name = "test_cookie",
          .attributes = "Secure; Domain=" + GURL(url_string).GetHost(),
      }},
      .key_id = key_id,
      .attestation_key_id = attestation_key_id,
  });
}

std::unique_ptr<Session> CreateSessionHelper(
    unexportable_keys::UnexportableKeyService& key_service,
    std::string_view url_string,
    std::string_view session_id,
    std::string_view origin = "https://foo.test",
    std::optional<unexportable_keys::UnexportableAttestationKeyId>
        attestation_key_id = std::nullopt) {
  return CreateSessionHelper(GenerateNewSigningKey(key_service), url_string,
                             session_id, origin, attestation_key_id);
}

proto::Session CreateSessionProto(
    unexportable_keys::UnexportableKeyService& key_service,
    const std::string& url_string,
    const std::string& session_id,
    const std::string& origin) {
  std::unique_ptr<Session> session =
      CreateSessionHelper(key_service, url_string, session_id, origin);
  proto::Session sproto = session->ToProto();
  unexportable_keys::UnexportableSigningKeyId key_id =
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
    store.SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);
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
            [&run_loop, session](unexportable_keys::ServiceErrorOr<
                                 unexportable_keys::UnexportableSigningKeyId>
                                     key_id_or_error) {
              session->set_unexportable_key_id(key_id_or_error);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void RestoreSessionAttestationKey(const SchemefulSite& site,
                                    Session* session) {
    base::RunLoop run_loop;
    store_->RestoreSessionAttestationKey(
        SessionKey{site, session->id()},
        base::BindLambdaForTesting(
            [&run_loop,
             session](unexportable_keys::ServiceErrorOr<
                      unexportable_keys::UnexportableAttestationKeyId>
                          key_id_or_error) {
              session->set_unexportable_attestation_key_id(key_id_or_error);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  crypto::ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<crypto::ScopedMockUnexportableKeyProvider>();
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               crypto::ScopedMockUnexportableKeyProvider>
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
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);
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
  session->set_unexportable_key_id(
      unexportable_keys::UnexportableSigningKeyId());
  store().SaveSession(net::SchemefulSite(GURL("https://foo.test")), *session,
                      SessionStore::SaveSessionMode::kNewSession);
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
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Modify the existing session and save it again to the store. The
  // save will fail if time advances past the expiry date, so use a 10
  // second margin of safety. This is arbitrary, as long as it's longer
  // than it takes to save a session.
  session->set_expiry_date(base::Time::Now() + base::Seconds(10));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

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
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);
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
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
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

  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
      features::kDeviceBoundSessions,
      {{features::kDeviceBoundSessionsSchemaVersion.name, "2"}});
  MimicRestart();

  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  EXPECT_TRUE(loaded_sessions.empty());
}

TEST_F(SessionStoreImplTest, DropHigherSchemaVersionSessions) {
  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
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

  AddScopedFeatureList().InitAndEnableFeatureWithParameters(
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
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify:
  // - entry with valid site is present in the output sessions map.
  // - entry with invalid site is not present and is included in the
  //   keys_to_delete list.
  EXPECT_THAT(sessions_map,
              ElementsAre(Pair(SessionKey{site2, Session::Id("session_id")},
                               NotNull())));
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, ElementsAre("about:blank"));
}

// Note: There are several reasons why a session may be invalid. We only
// use one of them here to test the pruning logic. The individual invalid
// reasons have been tested in SessionTest.FailCreateFromInvalidProto
// in file session_unittest.cc
TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidSession) {
  base::HistogramTester histograms;
  // Create an entry with 1 valid and 1 invalid session.
  proto::Session sproto1 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  // Create an invalid session.
  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_2", "https://foo.example.test");
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
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify that the invalid session is pruned while the valid session for
  // the same site is kept.
  EXPECT_THAT(keys_to_delete, IsEmpty());
  EXPECT_THAT(sites_to_update,
              ElementsAre(Pair(site.Serialize(),
                               Property(&proto::SiteSessions::sessions,
                                        ElementsAre(Key("session_1"))))));
  EXPECT_THAT(
      sessions_map,
      ElementsAre(Pair(SessionKey{site, Session::Id("session_1")}, NotNull())));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kInvalidSessionParams, 1);
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithSessionMissingWrappedKey) {
  base::HistogramTester histograms;
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
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify that the DB entry has been pruned in the output sessions map.
  EXPECT_THAT(sessions_map, IsEmpty());
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, ElementsAre(site.Serialize()));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kInvalidSessionParams, 1);
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithInvalidRefreshInitiator) {
  base::HistogramTester histograms;
  // Create an entry with an invalid refresh initiator.
  proto::Session sproto =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  sproto.add_allowed_refresh_initiators("a.*.example.test");

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_1"] = std::move(sproto);
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  // Run the table through the store's cleaning method.
  std::vector<std::string> keys_to_delete;
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify that the DB entry has been pruned in the output sessions map.
  EXPECT_THAT(sessions_map, IsEmpty());
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, ElementsAre(site.Serialize()));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kInvalidSessionParams, 1);
}

TEST_F(SessionStoreImplTest, LoadedEntryWithSessionIdMismatch) {
  base::HistogramTester histograms;
  // Create an entry where the map key does not match the session id in proto.
  proto::Session sproto =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_2"] = std::move(sproto);
  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  std::vector<std::string> keys_to_delete;
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // The session is loaded without pruning.
  EXPECT_THAT(
      sessions_map,
      ElementsAre(Pair(SessionKey{site, Session::Id("session_1")}, NotNull())));
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, IsEmpty());
  histograms.ExpectTotalCount("Net.DeviceBoundSessions.DeletionReason", 0);
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithExpiredSession) {
  base::HistogramTester histograms;
  // Create an entry with 1 valid and 1 expired session.
  proto::Session sproto1 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  // Create an expired session.
  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_2", "https://foo.example.test");
  base::Time expiry_date = base::Time::Now() - base::Days(1);
  sproto2.set_expiry_time(
      expiry_date.ToDeltaSinceWindowsEpoch().InMicroseconds());

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_1"] = std::move(sproto1);
  (*site_proto.mutable_sessions())["session_2"] = std::move(sproto2);

  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  std::vector<std::string> keys_to_delete;
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify that only the expired session is pruned and the valid session is
  // kept.
  EXPECT_THAT(keys_to_delete, IsEmpty());
  EXPECT_THAT(sites_to_update,
              ElementsAre(Pair(site.Serialize(),
                               Property(&proto::SiteSessions::sessions,
                                        ElementsAre(Key("session_1"))))));
  EXPECT_THAT(
      sessions_map,
      ElementsAre(Pair(SessionKey{site, Session::Id("session_1")}, NotNull())));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kExpired, 1);
}

TEST_F(SessionStoreImplTest, PruneLoadedEntryWithAllExpiredSessions) {
  base::HistogramTester histograms;
  // Create an entry where all sessions are expired.
  proto::Session sproto1 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  base::Time expiry_date1 = base::Time::Now() - base::Days(1);
  sproto1.set_expiry_time(
      expiry_date1.ToDeltaSinceWindowsEpoch().InMicroseconds());

  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_2", "https://foo.example.test");
  base::Time expiry_date2 = base::Time::Now() - base::Days(2);
  sproto2.set_expiry_time(
      expiry_date2.ToDeltaSinceWindowsEpoch().InMicroseconds());

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_1"] = std::move(sproto1);
  (*site_proto.mutable_sessions())["session_2"] = std::move(sproto2);

  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  std::vector<std::string> keys_to_delete;
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/true);

  // Verify that all sessions are pruned and the whole site key is scheduled for
  // deletion.
  EXPECT_THAT(sessions_map, IsEmpty());
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, ElementsAre(site.Serialize()));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kExpired, 2);
}

TEST_F(SessionStoreImplTest,
       CreateSessionsFromLoadedDataKeepExpiredWhenPruneDisabled) {
  base::HistogramTester histograms;
  // Create an entry with 1 valid and 1 expired session.
  proto::Session sproto1 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_1", "https://foo.example.test");
  proto::Session sproto2 =
      CreateSessionProto(unexportable_key_service(), "https://foo.example.test",
                         "session_2", "https://foo.example.test");
  base::Time expiry_date = base::Time::Now() - base::Days(1);
  sproto2.set_expiry_time(
      expiry_date.ToDeltaSinceWindowsEpoch().InMicroseconds());

  proto::SiteSessions site_proto;
  (*site_proto.mutable_sessions())["session_1"] = std::move(sproto1);
  (*site_proto.mutable_sessions())["session_2"] = std::move(sproto2);

  std::map<std::string, proto::SiteSessions> loaded_tbl;
  auto site = net::SchemefulSite(GURL("https://foo.example.test"));
  loaded_tbl[site.Serialize()] = std::move(site_proto);

  std::vector<std::string> keys_to_delete;
  std::map<std::string, proto::SiteSessions> sites_to_update;
  SessionStore::SessionsMap sessions_map =
      SessionStoreImpl::CreateSessionsFromLoadedData(
          loaded_tbl, keys_to_delete, sites_to_update,
          /*prune_expired_sessions=*/false);

  // Both sessions should be deserialized without pruning or metric emission.
  EXPECT_THAT(sites_to_update, IsEmpty());
  EXPECT_THAT(keys_to_delete, IsEmpty());
  EXPECT_THAT(
      sessions_map,
      ElementsAre(Pair(SessionKey{site, Session::Id("session_1")}, NotNull()),
                  Pair(SessionKey{site, Session::Id("session_2")}, NotNull())));
  histograms.ExpectTotalCount("Net.DeviceBoundSessions.DeletionReason", 0);
}

TEST_F(SessionStoreImplTest, LoadSessionsPrunesExpiredSessionsOnRestart) {
  base::HistogramTester histograms;
  CreateStoreAndLoadSessions();

  // Save session 1_1 for site 1 with a brief expiry time.
  std::unique_ptr<Session> session1_1 = CreateSessionHelper(
      unexportable_key_service(), "https://foo.test", "session1_1");
  session1_1->set_expiry_date(base::Time::Now() + base::Hours(1));
  auto site1 = net::SchemefulSite(GURL("https://foo.test"));
  store().SaveSession(site1, *session1_1,
                      SessionStore::SaveSessionMode::kNewSession);

  // Save session 1_2 for site 1 with a long expiry time.
  std::unique_ptr<Session> session1_2 =
      CreateSessionHelper(unexportable_key_service(), "https://foo.test",
                          "session1_2", "https://foo.test");
  session1_2->set_expiry_date(base::Time::Now() + base::Days(10));
  store().SaveSession(site1, *session1_2,
                      SessionStore::SaveSessionMode::kNewSession);

  // Save session 2 for site 2 with a brief expiry time.
  std::unique_ptr<Session> session2 =
      CreateSessionHelper(unexportable_key_service(), "https://bar.test",
                          "session2", "https://bar.test");
  session2->set_expiry_date(base::Time::Now() + base::Hours(1));
  auto site2 = net::SchemefulSite(GURL("https://bar.test"));
  store().SaveSession(site2, *session2,
                      SessionStore::SaveSessionMode::kNewSession);

  EXPECT_EQ(store().GetAllSessions().size(), 3u);

  // Fast forward time past session 1_1 and session 2's expiry, but before
  // session 1_2's expiry.
  FastForwardBy(base::Hours(2));

  // GetAllSessions() should act as a pure read-only inspection and return all
  // cached sessions (including expired ones) without crashing or pruning.
  EXPECT_EQ(store().GetAllSessions().size(), 3u);
  histograms.ExpectTotalCount("Net.DeviceBoundSessions.DeletionReason", 0);

  // Restart and reload sessions from disk.
  MimicRestart();

  // On reload:
  // - site 1 has 1 expired session and 1 valid session, so site 1 is updated in
  //   the DB.
  // - site 2 has all sessions expired, so site 2 is deleted from the DB.
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  EXPECT_THAT(loaded_sessions,
              ElementsAre(Pair(SessionKey{site1, Session::Id("session1_2")},
                               NotNull())));
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kExpired, 2);
}

TEST_F(SessionStoreImplTest, GarbageCollectsStaleKeys) {
  base::HistogramTester histograms;
  AddScopedFeatureList().InitAndEnableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  crypto::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  CreateStoreAndLoadSessions();

  // The first two keys are known to the service, but the third key is stale.
  const std::vector<uint8_t> kWrappedKey1 = {1, 2, 3};
  const std::vector<uint8_t> kWrappedKey2 = {4, 5, 6};
  const std::vector<uint8_t> kStaleWrappedKey = {7, 8, 9};

  EXPECT_CALL(mock_key_provider, GetAllKeysSlowly).WillRepeatedly([=] {
    auto key1 = std::make_unique<crypto::MockUnexportableSigningKey>();
    auto key2 = std::make_unique<crypto::MockUnexportableSigningKey>();
    auto stale_key = std::make_unique<crypto::MockUnexportableSigningKey>();

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
      std::vector<unexportable_keys::UnexportableSigningKeyId>>>
      get_all_keys_future;
  unexportable_key_service().GetAllKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      get_all_keys_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(
      std::vector<unexportable_keys::UnexportableSigningKeyId> all_keys_ids,
      get_all_keys_future.Take());

  unexportable_keys::UnexportableSigningKeyId key_id_1(all_keys_ids[0]);
  unexportable_keys::UnexportableSigningKeyId key_id_2(all_keys_ids[1]);

  // Save two new sessions.
  static constexpr std::string_view kFooSite = "https://foo.test";
  static constexpr std::string_view kBarSite = "https://bar.test";
  store().SaveSession(
      net::SchemefulSite(GURL(kFooSite)),
      *CreateSessionHelper(key_id_1, kFooSite, "session1", kFooSite),
      SessionStore::SaveSessionMode::kNewSession);
  ASSERT_EQ(store().GetAllSessions().size(), 1u);

  store().SaveSession(
      net::SchemefulSite(GURL(kBarSite)),
      *CreateSessionHelper(key_id_2, kBarSite, "session2", kBarSite),
      SessionStore::SaveSessionMode::kNewSession);
  ASSERT_EQ(store().GetAllSessions().size(), 2u);

  // Finish loading the sessions, and wait for the stale key to be deleted.
  EXPECT_CALL(mock_key_provider, DeleteKeysSlowly).WillOnce([&](auto keys) {
    auto wrapped_keys =
        base::ToVector(keys, [](auto* key) { return key->GetWrappedKey(); });
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
  AddScopedFeatureList().InitAndDisableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  crypto::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  EXPECT_CALL(mock_key_provider, GetAllKeysSlowly).Times(0);
  EXPECT_CALL(mock_key_provider, DeleteAllKeysSlowly).Times(0);

  CreateStoreAndLoadSessions();
}

TEST_F(SessionStoreImplTest, SaveAndLoadSessionWithAttestationKey) {
  base::HistogramTester histograms;
  CreateStoreAndLoadSessions();

  // Generate mock signing and attestation keys.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  // Create a session with both keys.
  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  ASSERT_TRUE(session);

  // Save the session.
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kSaveSessionKeySuccess, 1);

  // Verify it is saved in memory.
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Verify that the database proto contains the correct wrapped attestation key
  // bytes.
  proto::SiteSessions site_proto;
  std::string site_str = site.Serialize();
  ASSERT_TRUE(store().session_data_->TryGetData(site_str, &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_EQ(session_proto->wrapped_attestation_key(),
            base::as_string_view(
                GetWrappedKey(unexportable_key_service(), attestation_key_id)));

  // Restart the store to simulate browser restart.
  MimicRestart();

  // Load sessions from disk.
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* loaded_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(loaded_session);

  // Verify that the loaded session does NOT have the wrapped attestation key in
  // memory, and the attestation key ID is kKeyNotReady.
  EXPECT_THAT(loaded_session->maybe_unexportable_attestation_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotReady));
}

TEST_F(SessionStoreImplTest, SaveSessionWithoutAttestationKey) {
  base::HistogramTester histograms;
  CreateStoreAndLoadSessions();

  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, std::nullopt);
  ASSERT_TRUE(session);

  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kNoAttestationKey, 1);

  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Verify database proto does NOT have wrapped_attestation_key.
  proto::SiteSessions site_proto;
  std::string site_str = site.Serialize();
  ASSERT_TRUE(store().session_data_->TryGetData(site_str, &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_FALSE(session_proto->has_wrapped_attestation_key());
}

TEST_F(SessionStoreImplTest, SaveSessionWithAttestationKeyWrappingFailure) {
  base::HistogramTester histograms;
  CreateStoreAndLoadSessions();

  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  // Use an empty/invalid attestation key ID to trigger wrapping failure.
  unexportable_keys::UnexportableAttestationKeyId invalid_attestation_key_id;

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, invalid_attestation_key_id);
  ASSERT_TRUE(session);

  // Save the session.
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kGetWrappedKeyFailure, 1);

  // Verify that the session was saved despite the attestation key wrapping
  // failure.
  EXPECT_EQ(store().GetAllSessions().size(), 1u);

  // Verify database proto does NOT have wrapped_attestation_key.
  proto::SiteSessions site_proto;
  std::string site_str = site.Serialize();
  ASSERT_TRUE(store().session_data_->TryGetData(site_str, &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_FALSE(session_proto->has_wrapped_attestation_key());

  // Verify that when restored, the session has no attestation key.
  MimicRestart();
  auto loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);
  auto loaded_it =
      loaded_sessions.find(SessionKey{site, Session::Id(kSessionId)});
  ASSERT_NE(loaded_it, loaded_sessions.end());
  EXPECT_EQ(loaded_it->second->maybe_unexportable_attestation_key_id(),
            std::nullopt);
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionPreservesAttestationKey) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  std::vector<uint8_t> expected_wrapped_attestation_key =
      GetWrappedKey(unexportable_key_service(), attestation_key_id);

  // 2. Restart and load it (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);
  EXPECT_THAT(restored_session->maybe_unexportable_attestation_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotReady));

  // 3. Restore the binding key before saving.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // Modify some other property and save.
  restored_session->set_expiry_date(base::Time::Now() + base::Seconds(10));
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kRefresh);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kKeyNotReadyCopiedOldKey, 1);

  // 4. Verify that the DB proto STILL has the attestation key.
  {
    proto::SiteSessions site_proto;
    ASSERT_TRUE(
        store().session_data_->TryGetData(site.Serialize(), &site_proto));
    const proto::Session* session_proto =
        base::FindOrNull(site_proto.sessions(), kSessionId);
    ASSERT_TRUE(session_proto);
    EXPECT_EQ(session_proto->wrapped_attestation_key(),
              base::as_string_view(expected_wrapped_attestation_key));
  }

  // 5. Restart again and verify that the attestation key is still there.
  MimicRestart();
  SessionStore::SessionsMap reloaded_sessions = LoadSessions();
  ASSERT_EQ(reloaded_sessions.size(), 1u);

  Session* reloaded_session = reloaded_sessions.begin()->second.get();
  ASSERT_TRUE(reloaded_session);
  EXPECT_THAT(reloaded_session->maybe_unexportable_attestation_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotReady));

  {
    proto::SiteSessions site_proto;
    ASSERT_TRUE(
        store().session_data_->TryGetData(site.Serialize(), &site_proto));
    const proto::Session* session_proto =
        base::FindOrNull(site_proto.sessions(), kSessionId);
    ASSERT_TRUE(session_proto);
    EXPECT_EQ(session_proto->wrapped_attestation_key(),
              base::as_string_view(expected_wrapped_attestation_key));
  }
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionWithNewAttestationKey) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load it (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // 3. Generate a NEW attestation key and set it.
  unexportable_keys::UnexportableAttestationKeyId new_attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());
  restored_session->set_unexportable_attestation_key_id(new_attestation_key_id);

  // Save the session.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kSaveSessionKeySuccess, 1);

  // 4. Verify that the DB proto now has the NEW wrapped attestation key.
  proto::SiteSessions site_proto;
  ASSERT_TRUE(store().session_data_->TryGetData(site.Serialize(), &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_EQ(session_proto->wrapped_attestation_key(),
            base::as_string_view(GetWrappedKey(unexportable_key_service(),
                                               new_attestation_key_id)));
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionClearsAttestationKey) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load it (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // 3. Set the attestation key ID to std::nullopt (explicitly clearing it).
  restored_session->set_unexportable_attestation_key_id(std::nullopt);

  // Save the session.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kNoAttestationKey, 1);

  // 4. Verify that the DB proto no longer has the wrapped attestation key.
  proto::SiteSessions site_proto;
  ASSERT_TRUE(store().session_data_->TryGetData(site.Serialize(), &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_FALSE(session_proto->has_wrapped_attestation_key());
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionSiteNotFound) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load it (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // 3. Manually delete the session from the store. This will delete the site
  // entry since it's the only session for that site.
  store().DeleteSession(SessionKey{site, Session::Id(kSessionId)});

  // 4. Try to save the restored session again.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kRefresh);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kKeyNotReadyNoSiteInDb, 1);
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionSessionNotFound) {
  CreateStoreAndLoadSessions();

  // 1. Create and save TWO sessions for the same site (one with attestation
  // key, one without).
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  auto site = net::SchemefulSite(GURL(kSite));

  std::unique_ptr<Session> session1 = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  store().SaveSession(site, *session1,
                      SessionStore::SaveSessionMode::kNewSession);

  std::unique_ptr<Session> session2 = CreateSessionHelper(
      signing_key_id, kSite, "session2", kSite, std::nullopt);
  store().SaveSession(site, *session2,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 2u);

  auto it1 = loaded_sessions.find(SessionKey{site, Session::Id(kSessionId)});
  ASSERT_NE(it1, loaded_sessions.end());
  Session* restored_session1 = it1->second.get();
  ASSERT_TRUE(restored_session1);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session1);
  ASSERT_TRUE(restored_session1->unexportable_key_id().has_value());

  // 3. Delete session1 from the database. The site entry still exists because
  // session2 is still in the database.
  store().DeleteSession(SessionKey{site, Session::Id(kSessionId)});

  // 4. Try to save restored session1 again.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session1,
                      SessionStore::SaveSessionMode::kRefresh);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kKeyNotReadyNoSessionInDb, 1);
}

TEST_F(SessionStoreImplTest, SaveRestoredSessionNoOldKeyToCopy) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, attestation_key_id);
  auto site = net::SchemefulSite(GURL(kSite));
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // 3. Manually clear the wrapped attestation key from the database proto
  // cache, simulating database corruption or external modification.
  proto::SiteSessions site_proto;
  ASSERT_TRUE(store().session_data_->TryGetData(site.Serialize(), &site_proto));
  proto::Session* old_session =
      base::FindOrNull(*site_proto.mutable_sessions(), kSessionId);
  ASSERT_TRUE(old_session);
  old_session->clear_wrapped_attestation_key();
  store().session_data_->UpdateData(site.Serialize(), site_proto);

  // 4. Try to save the restored session again.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kRefresh);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kKeyNotReadyNoOldKeyToCopy, 1);
}

TEST_F(SessionStoreImplTest,
       SaveRestoredSessionWithoutRefreshFailsToPreserveAttestationKey) {
  CreateStoreAndLoadSessions();

  // 1. Create and save a session with an attestation key.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  auto session = CreateSessionHelper(signing_key_id, kSite, kSessionId, kSite,
                                     attestation_key_id);
  net::SchemefulSite site{GURL(kSite)};
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // 2. Restart and load (restored state).
  MimicRestart();
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);

  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Restore the binding key.
  RestoreSessionBindingKey(site, restored_session);
  ASSERT_TRUE(restored_session->unexportable_key_id().has_value());

  // 3. Try to save the restored session again with is_refresh = false.
  base::HistogramTester histograms;
  store().SaveSession(site, *restored_session,
                      SessionStore::SaveSessionMode::kNewSession);

  // It should fail with kUnexpectedError because kKeyNotReady is unexpected
  // for a non-refresh save.
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kUnexpectedError, 1);

  // 4. Verify that the DB proto no longer has the wrapped attestation key.
  proto::SiteSessions site_proto;
  ASSERT_TRUE(store().session_data_->TryGetData(site.Serialize(), &site_proto));
  const proto::Session* session_proto =
      base::FindOrNull(site_proto.sessions(), kSessionId);
  ASSERT_TRUE(session_proto);
  EXPECT_FALSE(session_proto->has_wrapped_attestation_key());
}

TEST_F(SessionStoreImplTest, SaveSessionWithUnexpectedAttestationKeyError) {
  CreateStoreAndLoadSessions();

  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());

  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kSite, kSessionId, kSite, std::nullopt);
  ASSERT_TRUE(session);

  // 1. Manually set the attestation key ID to an unexpected error (like
  // kCryptoApiFailed).
  session->set_unexportable_attestation_key_id(
      base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed));

  // 2. Save the session.
  auto site = net::SchemefulSite(GURL(kSite));
  base::HistogramTester histograms;
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.AttestationKeySaveOutcome",
      AttestationKeySaveOutcome::kUnexpectedError, 1);
}

TEST_F(SessionStoreImplTest, GarbageCollectsStaleKeysWithAttestation) {
  base::HistogramTester histograms;
  AddScopedFeatureList().InitAndEnableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  crypto::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  CreateStoreAndLoadSessions();

  // Keys:
  // 1. Active signing key (should be protected)
  // 2. Active attestation key (should be protected)
  // 3. Stale signing key (should be deleted)
  // 4. Stale attestation key (should be deleted)
  const std::vector<uint8_t> kActiveSigningWrapped = {1, 2, 3};
  const std::vector<uint8_t> kActiveAttestationWrapped = {4, 5, 6};
  const std::vector<uint8_t> kStaleSigningWrapped = {7, 8, 9};
  const std::vector<uint8_t> kStaleAttestationWrapped = {10, 11, 12};

  EXPECT_CALL(mock_key_provider, GetAllKeysSlowly).WillRepeatedly([=] {
    auto key1 = std::make_unique<crypto::MockUnexportableSigningKey>();
    auto key2 = std::make_unique<crypto::MockUnexportableSigningKey>();
    auto key3 = std::make_unique<crypto::MockUnexportableSigningKey>();
    auto key4 = std::make_unique<crypto::MockUnexportableSigningKey>();

    ON_CALL(*key1, GetWrappedKey).WillByDefault(Return(kActiveSigningWrapped));
    ON_CALL(*key2, GetWrappedKey)
        .WillByDefault(Return(kActiveAttestationWrapped));
    ON_CALL(*key3, GetWrappedKey).WillByDefault(Return(kStaleSigningWrapped));
    ON_CALL(*key4, GetWrappedKey)
        .WillByDefault(Return(kStaleAttestationWrapped));

    return base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
        std::move(key1),
        std::move(key2),
        std::move(key3),
        std::move(key4),
    });
  });

  // Obtain the corresponding key ids.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      std::vector<unexportable_keys::UnexportableSigningKeyId>>>
      get_all_keys_future;
  unexportable_key_service().GetAllKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      get_all_keys_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(
      std::vector<unexportable_keys::UnexportableSigningKeyId> all_keys_ids,
      get_all_keys_future.Take());
  ASSERT_EQ(all_keys_ids.size(), 4u);

  unexportable_keys::UnexportableSigningKeyId active_signing_id(
      all_keys_ids[0]);
  unexportable_keys::UnexportableAttestationKeyId active_attestation_id(
      all_keys_ids[1]);

  // Save a session with the active signing and attestation keys.
  static constexpr std::string_view kFooSite = "https://foo.test";
  store().SaveSession(
      net::SchemefulSite(GURL(kFooSite)),
      *CreateSessionHelper(active_signing_id, kFooSite, kSessionId, kFooSite,
                           active_attestation_id),
      SessionStore::SaveSessionMode::kNewSession);
  ASSERT_EQ(store().GetAllSessions().size(), 1u);

  // Wait for the stale keys to be deleted.
  EXPECT_CALL(mock_key_provider, DeleteKeysSlowly).WillOnce([&](auto keys) {
    auto wrapped_keys =
        base::ToVector(keys, [](auto* key) { return key->GetWrappedKey(); });
    EXPECT_THAT(wrapped_keys,
                testing::UnorderedElementsAre(kStaleSigningWrapped,
                                              kStaleAttestationWrapped));
    return wrapped_keys.size();
  });

  // Advance time to allow StartGarbageCollection to run.
  FastForwardBy(kGarbageCollectionDelay);

  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "TotalKeyCount",
      4, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "UsedKeyCount",
      2, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyCount",
      2, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyDeletionCount",
      2, 1);
}

TEST_F(SessionStoreImplTest, RestoreSessionAttestationKeySuccess) {
  CreateStoreAndLoadSessions();

  // Create valid keys.
  unexportable_keys::UnexportableSigningKeyId signing_key_id =
      GenerateNewSigningKey(unexportable_key_service());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      GenerateNewAttestationKey(unexportable_key_service());

  // Save session with both keys.
  static constexpr std::string_view kFooSite = "https://foo.test";
  auto site = net::SchemefulSite(GURL(kFooSite));
  std::unique_ptr<Session> session = CreateSessionHelper(
      signing_key_id, kFooSite, kSessionId, kFooSite, attestation_key_id);
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // Mimic restart.
  MimicRestart();

  // Reload sessions.
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);
  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Initially, the attestation key is not ready.
  EXPECT_THAT(restored_session->maybe_unexportable_attestation_key_id(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotReady));

  // Restore the attestation key.
  RestoreSessionAttestationKey(site, restored_session);

  // Verify it was successfully restored and has the correct key ID.
  ASSERT_OK_AND_ASSIGN(
      std::optional<unexportable_keys::UnexportableAttestationKeyId>
          restored_key_id,
      restored_session->maybe_unexportable_attestation_key_id());
  ASSERT_TRUE(restored_key_id.has_value());
  EXPECT_EQ(*restored_key_id, attestation_key_id);
}

TEST_F(SessionStoreImplTest, RestoreSessionAttestationKeyNoKey) {
  CreateStoreAndLoadSessions();

  // Create session without an attestation key.
  static constexpr std::string_view kFooSite = "https://foo.test";
  auto site = net::SchemefulSite(GURL(kFooSite));
  std::unique_ptr<Session> session =
      CreateSessionHelper(unexportable_key_service(), kFooSite, kSessionId);
  store().SaveSession(site, *session,
                      SessionStore::SaveSessionMode::kNewSession);

  // Mimic restart.
  MimicRestart();

  // Reload sessions.
  SessionStore::SessionsMap loaded_sessions = LoadSessions();
  ASSERT_EQ(loaded_sessions.size(), 1u);
  Session* restored_session = loaded_sessions.begin()->second.get();
  ASSERT_TRUE(restored_session);

  // Initially, the attestation key is std::nullopt.
  EXPECT_THAT(restored_session->maybe_unexportable_attestation_key_id(),
              ValueIs(std::nullopt));

  // Restore the attestation key should fail.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableAttestationKeyId>>
      future;
  store().RestoreSessionAttestationKey(SessionKey{site, restored_session->id()},
                                       future.GetCallback());
  EXPECT_THAT(future.Get(),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
}

}  // namespace net::device_bound_sessions
