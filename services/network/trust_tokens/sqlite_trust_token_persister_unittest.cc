// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/sqlite_trust_token_persister.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/sqlite_proto/key_value_data.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

// This file contains tests specific to the SQLite persister. The class's basic
// functionality is also covered by the parameterized test
// TrustTokenPersisterUnittest.
//
// Test that writing, saving to disk, reinitializing, and reading yields the
// written value.
TEST(SQLiteTrustTokenPersister, PutReinitializeAndGet) {
  base::test::TaskEnvironment env;

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  auto origin = *SuitableTrustTokenOrigin::Create(GURL("https://a.com/"));
  {
    std::unique_ptr<SQLiteTrustTokenPersister> persister;
    SQLiteTrustTokenPersister::CreateForFilePath(
        base::SingleThreadTaskRunner::GetCurrentDefault(), temp_path,
        /*flush_delay_for_writes=*/base::TimeDelta(),
        base::BindLambdaForTesting(
            [&persister](std::unique_ptr<SQLiteTrustTokenPersister> created) {
              persister = std::move(created);
              base::RunLoop().Quit();
            }));
    env.RunUntilIdle();  // Allow initialization to complete.
    ASSERT_TRUE(persister);

    TrustTokenIssuerConfig config;
    TrustToken my_token;
    my_token.set_body("token token token");
    *config.add_tokens() = my_token;

    auto config_to_store = std::make_unique<TrustTokenIssuerConfig>(config);
    persister->SetIssuerConfig(origin, std::move(config_to_store));

    env.RunUntilIdle();  // Allow the write to persist.
  }

  std::unique_ptr<SQLiteTrustTokenPersister> persister;
  SQLiteTrustTokenPersister::CreateForFilePath(
      base::SingleThreadTaskRunner::GetCurrentDefault(), temp_path,
      /*flush_delay_for_writes=*/base::TimeDelta(),
      base::BindLambdaForTesting(
          [&persister](std::unique_ptr<SQLiteTrustTokenPersister> created) {
            persister = std::move(created);
            base::RunLoop().Quit();
          }));
  env.RunUntilIdle();  // Allow initialization to complete.
  ASSERT_TRUE(persister);

  auto got = persister->GetIssuerConfig(origin);
  ASSERT_TRUE(got);
  ASSERT_EQ(got->tokens_size(), 1);
  EXPECT_EQ(got->tokens(0).body(), "token token token");

  persister.reset();
  // Wait until the persister's TrustTokenDatabaseOwner finishes closing its
  // database asynchronously, so as not to leak after the test concludes.
  env.RunUntilIdle();

  base::DeleteFile(temp_path);
}

// Ensure that it's possible to create a Trust Tokens persister on top of a
// directory that does not already exist (regression test for
// crbug.com/1098019).
TEST(SQLiteTrustTokenPersister, NonexistentDirectory) {
  base::test::TaskEnvironment env;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("some-nonexistent-directory/my-database"));
  ASSERT_FALSE(base::PathExists(temp_path.DirName()));

  std::unique_ptr<SQLiteTrustTokenPersister> persister;
  SQLiteTrustTokenPersister::CreateForFilePath(
      base::SingleThreadTaskRunner::GetCurrentDefault(), temp_path,
      /*flush_delay_for_writes=*/base::TimeDelta(),
      base::BindLambdaForTesting(
          [&persister](std::unique_ptr<SQLiteTrustTokenPersister> created) {
            persister = std::move(created);
            base::RunLoop().Quit();
          }));
  env.RunUntilIdle();  // Allow initialization to complete.
  ASSERT_TRUE(persister);

  persister.reset();
  // Wait until the persister's TrustTokenDatabaseOwner finishes closing its
  // database asynchronously, so as not to leak after the test concludes.
  env.RunUntilIdle();
}

}  // namespace network
