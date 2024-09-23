// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_persister.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/sqlite_trust_token_persister.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::EqualsProto;
using ::testing::IsNull;
using ::testing::Pointee;

namespace network {

namespace {

// Some arbitrary time in microseconds since windows epoch. This is a
// timestamp before begin timestamp to be used in time filters. Tokens
// and redemtion records created at this time should not be deleted.
const base::Time before_begin =
    base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(12345));
// 1 microsecond after before_begin, this will be used as begin time
// in time filters.
const base::Time begin_time = before_begin + base::Microseconds(1);
// 42 seconds later than begin_time (in microseconds since windows epoch). This
// will be used as creation time for token and redemption records.  Data with
// this creation time will get deleted.
const base::Time time_in_window = begin_time + base::Seconds(42);
// 1 second after time_in_window, this will be used as end time in time
// filters.
const base::Time end_time = time_in_window + base::Seconds(1);
// 1 microseconds later than end time (in microseconds since windows epoch).
// This will be used as a creation time, data created at this time
// should not be deleted.
const base::Time after_end_time = end_time + base::Microseconds(1);

// Helper for creating a Timestamp object from a number of microseconds
const auto TimestampFromMicros = [](int64_t micros) -> Timestamp {
  return internal::TimeToTimestamp(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(micros)));
};

// A matcher that returns true if origin == origin_to_delete.
// This is used to create matchers in tests.
const auto GenericKeyMatcher =
    [](const SuitableTrustTokenOrigin& origin,
       const SuitableTrustTokenOrigin& origin_to_delete) -> bool {
  return (origin == origin_to_delete);
};

// A matcher that returns true if creation time is between begin and end
// time. This is used to create time matchers in tests.
const auto GenericTimeMatcher = [](const base::Time& begin_time,
                                   const base::Time& end_time,
                                   const base::Time& creation_time) -> bool {
  const base::TimeDelta creation_delta =
      creation_time.ToDeltaSinceWindowsEpoch();
  const base::TimeDelta begin_delta = begin_time.ToDeltaSinceWindowsEpoch();
  const base::TimeDelta end_delta = end_time.ToDeltaSinceWindowsEpoch();
  if ((creation_delta < begin_delta) || (creation_delta > end_delta)) {
    return false;
  }
  return true;
};

const auto AlwaysFalseKeyMatcher = base::BindRepeating(
    [](const SuitableTrustTokenOrigin& origin) -> bool { return false; });

const auto AlwaysFalseTimeMatcher = base::BindRepeating(
    [](const base::Time& creation_time) -> bool { return false; });

const auto AlwaysTrueKeyMatcher = base::BindRepeating(
    [](const SuitableTrustTokenOrigin& origin) -> bool { return true; });

const auto AlwaysTrueTimeMatcher = base::BindRepeating(
    [](const base::Time& creation_time) -> bool { return true; });

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
        base::SingleThreadTaskRunner::GetCurrentDefault(),
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
        base::SingleThreadTaskRunner::GetCurrentDefault(),
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
    NOTREACHED_IN_MIGRATION();
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
  *config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *config.mutable_last_redemption() = TimestampFromMicros(200);

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

TYPED_TEST(TrustTokenPersisterTest, CallDeleteOnEmptyPersister) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  EXPECT_FALSE(
      persister->DeleteForOrigins(AlwaysTrueKeyMatcher, AlwaysTrueTimeMatcher));
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, DeletesIssuerToplevelKeyedDataNoRR) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create pair config that has no redemption record
  TrustTokenIssuerToplevelPairConfig pair_config;
  *pair_config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *pair_config.mutable_last_redemption() = TimestampFromMicros(200);

  // set config
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));
  env.RunUntilIdle();

  // When there is no RR, config should be deleted no matter what time matcher
  // returns.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer), AlwaysFalseTimeMatcher));
  env.RunUntilIdle();
  EXPECT_FALSE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DeletesIssuerToplevelKeyedDataHasRRNoCreationTime) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create pair config that has no redemption record
  TrustTokenIssuerToplevelPairConfig pair_config;
  *pair_config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *pair_config.mutable_last_redemption() = TimestampFromMicros(200);

  // set redemption record
  TrustTokenRedemptionRecord rr;
  rr.set_body("rr body");
  rr.set_token_verification_key("key");
  rr.set_lifetime(1234567);
  *(pair_config.mutable_redemption_record()) = rr;

  // set config
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // Config should not be deleted when key is not a match.
  EXPECT_FALSE(persister->DeleteForOrigins(AlwaysFalseKeyMatcher,
                                           AlwaysTrueTimeMatcher));
  env.RunUntilIdle();
  EXPECT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));
  env.RunUntilIdle();

  // Config should be deleted when redemption record does not have creation
  // time.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, toplevel),
      AlwaysFalseTimeMatcher));
  env.RunUntilIdle();
  EXPECT_FALSE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DeletesIssuerToplevelKeyedDataHasRRHasCreationTimeNoMatch) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create pair config that has no redemption record
  TrustTokenIssuerToplevelPairConfig pair_config;
  *pair_config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *pair_config.mutable_last_redemption() = TimestampFromMicros(100);

  // set redemption record
  TrustTokenRedemptionRecord rr;
  rr.set_body("rr body");
  rr.set_token_verification_key("key");
  rr.set_lifetime(1234567);
  *rr.mutable_creation_time() = internal::TimeToTimestamp(before_begin);
  *(pair_config.mutable_redemption_record()) = rr;

  // set config
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // Creation time is before begin time. Should not delete.
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();
  EXPECT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // set creation time to after end
  *pair_config.mutable_redemption_record()->mutable_creation_time() =
      internal::TimeToTimestamp(after_end_time);
  env.RunUntilIdle();

  // Creation time is after end time. Should not delete.
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();
  EXPECT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DeletesIssuerToplevelKeyedDataHasRRHasCreationTimeMatches) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create pair config that has no redemption record
  TrustTokenIssuerToplevelPairConfig pair_config;
  *pair_config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *pair_config.mutable_last_redemption() = TimestampFromMicros(200);

  // set redemption record
  TrustTokenRedemptionRecord rr;
  rr.set_body("rr body");
  rr.set_token_verification_key("key");
  rr.set_lifetime(1234567);
  *rr.mutable_creation_time() = internal::TimeToTimestamp(time_in_window);
  *(pair_config.mutable_redemption_record()) = rr;

  // set config
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(
      issuer, toplevel,
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(pair_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  // Creation time is before begin time. Should not delete.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, toplevel),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();
  EXPECT_FALSE(persister->GetIssuerToplevelPairConfig(issuer, toplevel));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, DeletesIssuerKeyedDataKeyNoTokens) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  TrustTokenIssuerConfig issuer_config;

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Delete should return false. Key matcher returns false.
  EXPECT_FALSE(persister->DeleteForOrigins(AlwaysFalseKeyMatcher,
                                           AlwaysTrueTimeMatcher));
  env.RunUntilIdle();
  EXPECT_TRUE(persister->GetIssuerConfig(issuer));

  // Key matches, issuer config has no tokens. Deletes config that
  // does not have any tokens no matter what time matcher returns.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer), AlwaysFalseTimeMatcher));
  EXPECT_FALSE(persister->GetIssuerConfig(issuer));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DeletesIssuerKeyedDataKeyHasTokenNoCreationTime) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create issuer config
  TrustTokenIssuerConfig issuer_config;

  // set token
  TrustToken* token = issuer_config.add_tokens();
  token->set_signing_key("key");

  // set issuer config
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Delete token that does not have creation time. Delete config since no
  // tokens left.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer), AlwaysFalseTimeMatcher));
  env.RunUntilIdle();
  EXPECT_FALSE(persister->GetIssuerConfig(issuer));

  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DeletesIssuerKeyedDataKeyHasTokenWithCreationTime) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();

  // create issuer config
  TrustTokenIssuerConfig issuer_config;

  // set token
  TrustToken* token = issuer_config.add_tokens();
  token->set_signing_key("key");
  *token->mutable_creation_time() = internal::TimeToTimestamp(before_begin);

  // set issuer config
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Do not delete token created before begin time.
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();
  EXPECT_TRUE(persister->GetIssuerConfig(issuer));

  // add token created between begin and end
  issuer_config.clear_tokens();
  token = issuer_config.add_tokens();
  token->set_signing_key("key");
  *token->mutable_creation_time() = internal::TimeToTimestamp(time_in_window);
  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();

  // Delete token created between begin and end time
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();
  EXPECT_FALSE(persister->GetIssuerConfig(issuer));

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
  EXPECT_FALSE(persister->DeleteForOrigins(AlwaysFalseKeyMatcher,
                                           AlwaysTrueTimeMatcher));

  // Deleting with a matcher matching |toplevel| should delete the data, no
  // matter what time matcher returns.  Time matcher does not matter. Top level
  // config does not have time data.
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, toplevel),
      AlwaysFalseTimeMatcher));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_FALSE(persister->GetToplevelConfig(toplevel));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, RetrievesAvailableTrustTokens) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  auto result = persister->GetStoredTrustTokenCounts();
  EXPECT_EQ(result.size(), 0ul);

  TrustTokenIssuerConfig config;
  TrustToken my_token;
  my_token.set_body("token token token");
  *my_token.mutable_creation_time() = internal::TimeToTimestamp(time_in_window);
  *config.add_tokens() = my_token;

  auto config_to_store = std::make_unique<TrustTokenIssuerConfig>(config);
  auto origin = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  persister->SetIssuerConfig(origin, std::move(config_to_store));

  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  result = persister->GetStoredTrustTokenCounts();

  EXPECT_EQ(result.size(), 1ul);
  EXPECT_EQ(result.begin()->first, origin);
  EXPECT_EQ(result.begin()->second, 1);

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           RetrievesRedemptionRecordsByIssuerToplevelPair) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.
  auto toplevel_a = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto toplevel_b = *SuitableTrustTokenOrigin::Create(GURL("https://b.com/"));
  auto issuer_a =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer_a.com/"));
  auto issuer_b =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer_b.com/"));
  auto result = persister->GetRedemptionRecords();
  EXPECT_TRUE(result.empty()) << result.size();

  TrustTokenIssuerToplevelPairConfig config;
  TrustTokenRedemptionRecord rr_a;

  Timestamp last_redemption = TimestampFromMicros(200);
  *rr_a.mutable_creation_time() = internal::TimeToTimestamp(before_begin);
  *config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *config.mutable_last_redemption() = last_redemption;
  *config.mutable_redemption_record() = rr_a;

  auto config_to_store =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(config);
  persister->SetIssuerToplevelPairConfig(issuer_a, toplevel_a,
                                         std::move(config_to_store));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  result = persister->GetRedemptionRecords();

  // Verify initial redemption record
  EXPECT_EQ(result.size(), 1ul);
  EXPECT_TRUE(result.contains(issuer_a.origin()));
  EXPECT_EQ(result[issuer_a][0]->toplevel_origin, toplevel_a.origin());
  EXPECT_EQ(result[issuer_a][0]->last_redemption,
            internal::TimestampToTime(last_redemption));

  TrustTokenRedemptionRecord rr_b;
  *rr_b.mutable_creation_time() = internal::TimeToTimestamp(before_begin);
  *config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *config.mutable_last_redemption() = last_redemption;
  *config.mutable_redemption_record() = rr_b;

  // Add unique issuer/toplevel pair
  auto config_to_store_b =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(config);
  persister->SetIssuerToplevelPairConfig(issuer_b, toplevel_b,
                                         std::move(config_to_store_b));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  result = persister->GetRedemptionRecords();
  EXPECT_EQ(result.size(), 2ul);
  EXPECT_TRUE(result.contains(issuer_b.origin()));
  EXPECT_EQ(result[issuer_b][0]->toplevel_origin, toplevel_b.origin());
  EXPECT_EQ(result[issuer_b][0]->last_redemption,
            internal::TimestampToTime(last_redemption));

  // Add new redemption record for an existing issuer
  auto config_to_store_c =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(config);
  persister->SetIssuerToplevelPairConfig(issuer_a, toplevel_b,
                                         std::move(config_to_store_c));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  // Verify existing issuer list updated
  result = persister->GetRedemptionRecords();
  EXPECT_EQ(result.size(), 2ul);
  EXPECT_EQ(result[issuer_a].size(), 2ul);

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, SomeTokensAreOutOfTimeWindow) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerConfig issuer_config;
  // add tokens
  {
    TrustToken* first_token = issuer_config.add_tokens();
    first_token->set_signing_key("key");
    first_token->set_body("before begin time");
    *first_token->mutable_creation_time() =
        internal::TimeToTimestamp(before_begin);

    TrustToken* second_token = issuer_config.add_tokens();
    second_token->set_signing_key("key");
    second_token->set_body("between begin and end time");
    *second_token->mutable_creation_time() =
        internal::TimeToTimestamp(time_in_window);

    TrustToken* third_token = issuer_config.add_tokens();
    third_token->set_signing_key("key");
    third_token->set_body("after end");
    *third_token->mutable_creation_time() =
        internal::TimeToTimestamp(after_end_time);
  }

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  EXPECT_TRUE(persister->GetIssuerConfig(issuer));

  // check first and third tokens are in store
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens().size(), 2);
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens()[0].body(),
            "before begin time");
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens()[1].body(),
            "after end");

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, AllTokensAreWithinTimeWindow) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerConfig issuer_config;

  // add tokens within window
  for (int i = 0; i < 5; ++i) {
    TrustToken* b_token = issuer_config.add_tokens();
    b_token->set_signing_key("key");
    b_token->set_body("token towards begin time");
    *b_token->mutable_creation_time() =
        internal::TimeToTimestamp(begin_time + base::Microseconds(i));

    TrustToken* e_token = issuer_config.add_tokens();
    e_token->set_signing_key("key");
    e_token->set_body("token towards end time");
    *e_token->mutable_creation_time() =
        internal::TimeToTimestamp(end_time - base::Microseconds(i));
  }
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Should delete all tokens
  EXPECT_TRUE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  // All tokens are within the time window, this results issuer config
  // being deleted.
  EXPECT_FALSE(persister->GetIssuerConfig(issuer));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest, AllTokensAreOutOfTimeWindow) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerConfig issuer_config;
  // add tokens
  {
    TrustToken* first_token = issuer_config.add_tokens();
    first_token->set_signing_key("key");
    first_token->set_body("before begin time");
    *first_token->mutable_creation_time() =
        internal::TimeToTimestamp(before_begin);

    TrustToken* second_token = issuer_config.add_tokens();
    second_token->set_signing_key("key");
    second_token->set_body("after end");
    *second_token->mutable_creation_time() =
        internal::TimeToTimestamp(after_end_time);
  }

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  persister->SetIssuerConfig(
      issuer, std::make_unique<TrustTokenIssuerConfig>(issuer_config));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  ASSERT_TRUE(persister->GetIssuerConfig(issuer));

  // Should not delete any token
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.
  EXPECT_TRUE(persister->GetIssuerConfig(issuer));

  // check the tokens
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens().size(), 2);
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens()[0].body(),
            "before begin time");
  EXPECT_EQ(persister->GetIssuerConfig(issuer)->tokens()[1].body(),
            "after end");

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

TYPED_TEST(TrustTokenPersisterTest,
           DoNotDeleteOutOfTimeWindowRedemptionRecord) {
  base::test::TaskEnvironment env;
  std::unique_ptr<TrustTokenPersister> persister = TypeParam::Create();
  env.RunUntilIdle();  // Give implementations with asynchronous initialization
                       // time to initialize.

  TrustTokenIssuerToplevelPairConfig config;
  *config.mutable_penultimate_redemption() = TimestampFromMicros(100);
  *config.mutable_last_redemption() = TimestampFromMicros(200);
  TrustTokenRedemptionRecord rr;
  rr.set_body("rr body");
  rr.set_token_verification_key("key");
  rr.set_lifetime(1234567);
  *rr.mutable_creation_time() = internal::TimeToTimestamp(before_begin);
  *(config.mutable_redemption_record()) = rr;

  auto config_to_store =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>(config);
  auto toplevel = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  persister->SetIssuerToplevelPairConfig(issuer, toplevel,
                                         std::move(config_to_store));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  // should not delete, since rr is before begin time
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, issuer),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  auto result = persister->GetIssuerToplevelPairConfig(issuer, toplevel);
  EXPECT_THAT(result, Pointee(EqualsProto(config)));

  // update redemptino record to have creation time after end time
  *rr.mutable_creation_time() = internal::TimeToTimestamp(after_end_time);
  *(result->mutable_redemption_record()) = rr;
  env.RunUntilIdle();

  // should not delete, since rr is after end time
  EXPECT_FALSE(persister->DeleteForOrigins(
      base::BindRepeating(GenericKeyMatcher, toplevel),
      base::BindRepeating(GenericTimeMatcher, begin_time, end_time)));
  env.RunUntilIdle();  // Give implementations with asynchronous write
                       // operations time to complete the operation.

  result = persister->GetIssuerToplevelPairConfig(issuer, toplevel);
  EXPECT_THAT(result, Pointee(EqualsProto(config)));

  // Some implementations of TrustTokenPersister may release resources
  // asynchronously at destruction time; manually free the persister and allow
  // this asynchronous release to occur, if any.
  persister.reset();
  env.RunUntilIdle();
}

// add tests when creation time is missing in tokens and rr

}  // namespace network
