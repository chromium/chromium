// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_persister.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/sqlite_trust_token_persister.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsNull;
using ::testing::Pointee;

namespace network {

namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class InMemoryTrustTokenPersisterFactory {
 public:
  static std::unique_ptr<TrustTokenPersister> Create() {
    return std::make_unique<InMemoryTrustTokenPersister>();
  }
};

class NoDatabaseSqliteTrustTokenPersisterFactory {
 public:
  static std::unique_ptr<TrustTokenPersister> Create() {
    std::unique_ptr<TrustTokenDatabaseOwner> owner;
    // Fail to open the database, in order to test that the in-memory fallback
    // on database error works as intended.
    TrustTokenDatabaseOwner::Create(
        /*db_opener=*/base::BindOnce([](sql::Database*) { return false; }),
        base::ThreadTaskRunnerHandle::Get(),
        /*flush_delay_for_writes=*/base::TimeDelta(),
        base::BindLambdaForTesting(
            [&owner](std::unique_ptr<TrustTokenDatabaseOwner> created) {
              owner = std::move(created);
              base::RunLoop().Quit();
            }));
    base::RunLoop().RunUntilIdle();
    CHECK(owner);
    return std::make_unique<SQLiteTrustTokenPersister>(std::move(owner));
  }
};

class EndToEndSqliteTrustTokenPersisterFactory {
 public:
  static std::unique_ptr<TrustTokenPersister> Create() {
    std::unique_ptr<TrustTokenDatabaseOwner> owner;
    TrustTokenDatabaseOwner::Create(
        /*db_opener=*/base::BindOnce(
            [](sql::Database* db) { return db->OpenInMemory(); }),
        base::ThreadTaskRunnerHandle::Get(),
        /*flush_delay_for_writes=*/base::TimeDelta(),
        base::BindLambdaForTesting(
            [&owner](std::unique_ptr<TrustTokenDatabaseOwner> created) {
              owner = std::move(created);
              base::RunLoop().Quit();
            }));
    base::RunLoop().RunUntilIdle();
    CHECK(owner);
    return std::make_unique<SQLiteTrustTokenPersister>(std::move(owner));
  }
};

}  // namespace

template <typename Factory>
class TrustTokenPersisterTest : public ::testing::Test {};

typedef ::testing::Types<InMemoryTrustTokenPersisterFactory,
                         NoDatabaseSqliteTrustTokenPersisterFactory,
                         EndToEndSqliteTrustTokenPersisterFactory>
    TrustTokenPersisterFactoryTypes;
class PersisterFactoryTypeNames {
 public:
  template <typename T>
  static std::string GetName(int) {
    if (std::is_same<T, InMemoryTrustTokenPersisterFactory>())
      return "InMemoryPersister";
    if (std::is_same<T, NoDatabaseSqliteTrustTokenPersisterFactory>())
      return "SQLitePersisterMemoryFallback";
    if (std::is_same<T, EndToEndSqliteTrustTokenPersisterFactory>())
      return "SQLitePersisterOnDisk";
    NOTREACHED();
    return "";
  }
};

TYPED_TEST_SUITE(TrustTokenPersisterTest,
                 TrustTokenPersisterFactoryTypes,
                 PersisterFactoryTypeNames);

TYPED_TEST(TrustTokenPersisterTest, NegativeResults) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  auto origin = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  EXPECT_THAT(persister->GetIssuerConfig(origin), IsNull());
  EXPECT_THAT(persister->GetToplevelConfig(origin), IsNull());
  EXPECT_THAT(persister->GetIssuerToplevelPairConfig(origin, origin), IsNull());

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, StoresIssuerConfigs) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerConfig config;
  TrustToken my_token;
  my_token.set_body("token token token");
  *config.add_tokens() = my_token;

  auto config_to_store = std::make_unique<TrustTokenIssuerConfig>(config);
  auto origin = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  persister->SetIssuerConfig(origin, std::move(config_to_store));

  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  auto result = persister->GetIssuerConfig(origin);

  EXPECT_THAT(result, Pointee(EqualsProto(config)));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, StoresToplevelConfigs) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenToplevelConfig config;
  *config.add_associated_issuers() = "an issuer";

  auto config_to_store = std::make_unique<TrustTokenToplevelConfig>(config);
  auto origin = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  persister->SetToplevelConfig(origin, std::move(config_to_store));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  auto result = persister->GetToplevelConfig(origin);

  EXPECT_THAT(result, Pointee(EqualsProto(config)));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, StoresIssuerToplevelPairConfigs) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerToplevelPairConfig config;
  config.set_last_redemption("five o'clock");

  auto config_to_store =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(config);
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(issuer, toplevel,
                                         std::move(config_to_store));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  auto result = persister->GetIssuerToplevelPairConfig(issuer, toplevel);

  EXPECT_THAT(result, Pointee(EqualsProto(config)));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, DeletesIssuerToplevelKeyedData) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerToplevelPairConfig pair_config;
  pair_config.set_last_redemption("five o'clock");

  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  // A matcher matching neither the issuer nor the top-level origin should not
  // delete (issuer, top level)-keyed data.
  EXPECT_FALSE(persister->DeleteForOrigins(base::BindLambdaForTesting(
      [&](const SuitableTrustTokenOrigin& origin) { return false; })));

  // A matcher matching the issuer should delete (issuer, top level)-keyed data.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindLambdaForTesting([&](const SuitableTrustTokenOrigin& origin) {
        return origin == issuer;
      })));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  EXPECT_FALSE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // A matcher matching the top-level origin should delete (issuer, top
  // level)-keyed data, too.
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));

  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindLambdaForTesting([&](const SuitableTrustTokenOrigin& origin) {
        return origin == toplevel;
      })));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  EXPECT_FALSE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, DeletesIssuerKeyedData) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerConfig issuer_config;
  issuer_config.add_tokens()->set_signing_key("key");

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Deleting with a matcher not matching |issuer| should no-op.
  EXPECT_FALSE(persister->DeleteForOrigins(base::BindLambdaForTesting(
      [&](const SuitableTrustTokenOrigin& origin) { return false; })));

  // Deleting with a matcher not matching |issuer| should delete data.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindLambdaForTesting([&](const SuitableTrustTokenOrigin& origin) {
        return origin == issuer;
      })));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_FALSE(persister->GetIssuerConfig(issuer));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, DeletesToplevelKeyedData) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenToplevelConfig toplevel_config;
  *toplevel_config.add_associated_issuers() = "some issuer";

  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  persister->SetToplevelConfig(
      toplevel, std::make_unique<TrustTokenToplevelConfig>(toplevel_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetToplevelConfig(toplevel));

  // Deleting with a matcher not matching |toplevel| should no-op.
  EXPECT_FALSE(persister->DeleteForOrigins(base::BindLambdaForTesting(
      [&](const SuitableTrustTokenOrigin& origin) { return false; })));

  // Deleting with a matcher matching |toplevel| should delete the data.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindLambdaForTesting([&](const SuitableTrustTokenOrigin& origin) {
        return origin == toplevel;
      })));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_FALSE(persister->GetToplevelConfig(toplevel));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

}  // namespace network
