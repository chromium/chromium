// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/fuchsia_cdm_manager.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <fuchsia/media/drm/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/fpromise/promise.h>
#include <map>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {
namespace {

namespace drm = ::fuchsia::media::drm;

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::WithArgs;

// This is a mock for the Chromium media::ProvisionFetcher (and not Fuchsia's
// similarly named ProvisioningFetcher protocol).
class MockProvisionFetcher : public ProvisionFetcher {
 public:
  MockProvisionFetcher() = default;
  ~MockProvisionFetcher() override = default;

  MOCK_METHOD(void,
              Retrieve,
              (const GURL& default_url,
               const std::string& request_data,
               ResponseCB response_cb),
              (override));
};

std::unique_ptr<ProvisionFetcher> CreateMockProvisionFetcher() {
  auto mock_provision_fetcher = std::make_unique<MockProvisionFetcher>();
  ON_CALL(*mock_provision_fetcher, Retrieve(_, _, _))
      .WillByDefault(WithArgs<2>(
          Invoke([](ProvisionFetcher::ResponseCB response_callback) {
            std::move(response_callback).Run(true, "response");
          })));
  return mock_provision_fetcher;
}

class MockKeySystem : public drm::testing::KeySystem_TestBase {
 public:
  MockKeySystem() = default;
  ~MockKeySystem() override = default;

  drm::KeySystemHandle AddBinding() { return bindings_.AddBinding(this); }
  fidl::BindingSet<drm::KeySystem>& bindings() { return bindings_; }

  void NotImplemented_(const std::string& name) override { FAIL() << name; }

  MOCK_METHOD(void,
              AddDataStore,
              (uint32_t data_store_id,
               drm::DataStoreParams params,
               AddDataStoreCallback callback),
              (override));
  MOCK_METHOD(
      void,
      CreateContentDecryptionModule2,
      (uint32_t data_store_id,
       fidl::InterfaceRequest<drm::ContentDecryptionModule> cdm_request),
      (override));

 private:
  fidl::BindingSet<drm::KeySystem> bindings_;
};

class FuchsiaCdmManagerTest : public ::testing::Test {
 public:
  FuchsiaCdmManagerTest() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  std::unique_ptr<FuchsiaCdmManager> CreateFuchsiaCdmManager(
      std::vector<std::string_view> key_systems,
      std::optional<uint64_t> cdm_data_quota_bytes = std::nullopt) {
    FuchsiaCdmManager::CreateKeySystemCallbackMap create_key_system_callbacks;

    for (const std::string_view& name : key_systems) {
      MockKeySystem& key_system = mock_key_systems_[name];
      create_key_system_callbacks.emplace(
          name, base::BindRepeating(&MockKeySystem::AddBinding,
                                    base::Unretained(&key_system)));
    }
    return std::make_unique<FuchsiaCdmManager>(
        std::move(create_key_system_callbacks), temp_dir_.GetPath(),
        cdm_data_quota_bytes);
  }

 protected:
  using MockKeySystemMap = std::map<std::string_view, MockKeySystem>;

  MockKeySystem& mock_key_system(const std::string_view& key_system_name) {
    return mock_key_systems_[key_system_name];
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  MockKeySystemMap mock_key_systems_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FuchsiaCdmManagerTest, NoKeySystems) {
  std::unique_ptr<FuchsiaCdmManager> cdm_manager = CreateFuchsiaCdmManager({});

  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_ptr.set_error_handler([&](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_NOT_FOUND);
    run_loop.Quit();
  });

  cdm_manager->CreateAndProvision(
      "com.key_system", url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  run_loop.Run();
}

TEST_F(FuchsiaCdmManagerTest, CreateAndProvision) {
  constexpr char kKeySystem[] = "com.key_system.a";
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({kKeySystem});

  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_ptr.set_error_handler([&](zx_status_t status) { run_loop.Quit(); });

  uint32_t added_data_store_id = 0;
  uint32_t cdm_data_store_id = 0;
  EXPECT_CALL(mock_key_system(kKeySystem), AddDataStore(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&](uint32_t data_store_id,
                     drm::KeySystem::AddDataStoreCallback callback) {
            added_data_store_id = data_store_id;
            callback(fpromise::ok());
          })));

  EXPECT_CALL(mock_key_system(kKeySystem), CreateContentDecryptionModule2(_, _))
      .WillOnce(SaveArg<0>(&cdm_data_store_id));

  cdm_manager->CreateAndProvision(
      kKeySystem, url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  run_loop.Run();

  EXPECT_NE(added_data_store_id, 0u);
  EXPECT_EQ(added_data_store_id, cdm_data_store_id);
}

TEST_F(FuchsiaCdmManagerTest, RecreateAfterDisconnect) {
  constexpr char kKeySystem[] = "com.key_system.a";
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({kKeySystem});

  uint32_t added_data_store_id = 0;
  EXPECT_CALL(mock_key_system(kKeySystem), AddDataStore(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&](uint32_t data_store_id,
                     drm::KeySystem::AddDataStoreCallback callback) {
            added_data_store_id = data_store_id;
            callback(fpromise::ok());
          })));

  // Create a CDM to force a KeySystem binding
  base::RunLoop create_run_loop;
  drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_ptr.set_error_handler(
      [&](zx_status_t status) { create_run_loop.Quit(); });
  cdm_manager->CreateAndProvision(
      kKeySystem, url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  create_run_loop.Run();
  ASSERT_EQ(mock_key_system(kKeySystem).bindings().size(), 1u);

  // Close the KeySystem's bindings and wait until empty
  base::RunLoop disconnect_run_loop;
  cdm_manager->set_on_key_system_disconnect_for_test_callback(
      base::BindLambdaForTesting([&](const std::string& key_system_name) {
        if (key_system_name == kKeySystem) {
          disconnect_run_loop.Quit();
        }
      }));
  mock_key_system(kKeySystem).bindings().CloseAll();
  disconnect_run_loop.Run();
  ASSERT_EQ(mock_key_system(kKeySystem).bindings().size(), 0u);

  EXPECT_CALL(mock_key_system(kKeySystem),
              AddDataStore(Eq(added_data_store_id), _, _))
      .WillOnce(
          WithArgs<2>(Invoke([](drm::KeySystem::AddDataStoreCallback callback) {
            callback(fpromise::ok());
          })));

  base::RunLoop recreate_run_loop;
  cdm_ptr.set_error_handler(
      [&](zx_status_t status) { recreate_run_loop.Quit(); });
  cdm_manager->CreateAndProvision(
      kKeySystem, url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  recreate_run_loop.Run();
  EXPECT_EQ(mock_key_system(kKeySystem).bindings().size(), 1u);
}

TEST_F(FuchsiaCdmManagerTest, SameOriginShareDataStore) {
  constexpr char kKeySystem[] = "com.key_system.a";
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({kKeySystem});

  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm1, cdm2;
  auto error_handler = [&](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    if (!cdm1.is_bound() && !cdm2.is_bound()) {
      run_loop.Quit();
    }
  };
  cdm1.set_error_handler(error_handler);
  cdm2.set_error_handler(error_handler);

  EXPECT_CALL(mock_key_system(kKeySystem), AddDataStore(Eq(1u), _, _))
      .WillOnce(
          WithArgs<2>(Invoke([](drm::KeySystem::AddDataStoreCallback callback) {
            callback(fpromise::ok());
          })));
  EXPECT_CALL(mock_key_system(kKeySystem),
              CreateContentDecryptionModule2(Eq(1u), _))
      .Times(2);

  url::Origin origin = url::Origin::Create(GURL("http://origin_a.com"));
  cdm_manager->CreateAndProvision(
      kKeySystem, origin, base::BindRepeating(&CreateMockProvisionFetcher),
      cdm1.NewRequest());
  cdm_manager->CreateAndProvision(
      kKeySystem, origin, base::BindRepeating(&CreateMockProvisionFetcher),
      cdm2.NewRequest());

  run_loop.Run();
}

TEST_F(FuchsiaCdmManagerTest, DifferentOriginDoNotShareDataStore) {
  constexpr char kKeySystem[] = "com.key_system.a";
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({kKeySystem});

  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm1, cdm2;
  auto error_handler = [&](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    if (!cdm1.is_bound() && !cdm2.is_bound()) {
      run_loop.Quit();
    }
  };
  cdm1.set_error_handler(error_handler);
  cdm2.set_error_handler(error_handler);

  EXPECT_CALL(mock_key_system(kKeySystem), AddDataStore(Eq(1u), _, _))
      .WillOnce(
          WithArgs<2>(Invoke([](drm::KeySystem::AddDataStoreCallback callback) {
            callback(fpromise::ok());
          })));
  EXPECT_CALL(mock_key_system(kKeySystem), AddDataStore(Eq(2u), _, _))
      .WillOnce(
          WithArgs<2>(Invoke([](drm::KeySystem::AddDataStoreCallback callback) {
            callback(fpromise::ok());
          })));
  EXPECT_CALL(mock_key_system(kKeySystem),
              CreateContentDecryptionModule2(Eq(1u), _))
      .Times(1);
  EXPECT_CALL(mock_key_system(kKeySystem),
              CreateContentDecryptionModule2(Eq(2u), _))
      .Times(1);

  url::Origin origin_a = url::Origin::Create(GURL("http://origin_a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("http://origin_b.com"));
  cdm_manager->CreateAndProvision(
      kKeySystem, origin_a, base::BindRepeating(&CreateMockProvisionFetcher),
      cdm1.NewRequest());
  cdm_manager->CreateAndProvision(
      kKeySystem, origin_b, base::BindRepeating(&CreateMockProvisionFetcher),
      cdm2.NewRequest());

  run_loop.Run();
}

void CreateDummyCdmDirectory(const base::FilePath& cdm_data_path,
                             std::string_view origin,
                             std::string_view key_system,
                             uint64_t size) {
  const base::FilePath path = cdm_data_path.Append(origin).Append(key_system);
  CHECK(base::CreateDirectory(path));
  if (size) {
    std::vector<uint8_t> zeroes(size);
    CHECK(base::WriteFile(path.Append("zeroes"), zeroes));
  }
}

// Verify that the least recently used CDM data directories are removed, until
// the quota is met.  Also verify that old directories are removed regardless
// of whether they are empty or not.
TEST_F(FuchsiaCdmManagerTest, CdmDataQuotaBytes) {
  constexpr uint64_t kTestQuotaBytes = 1024;
  constexpr char kOriginDirectory1[] = "origin1";
  constexpr char kOriginDirectory2[] = "origin2";
  constexpr char kKeySystemDirectory1[] = "key_system1";
  constexpr char kKeySystemDirectory2[] = "key_system2";
  constexpr char kEmptyKeySystemDirectory[] = "empty_key_system";

  // Create fake CDM data directories for two origins, each with two key
  // systems, with each directory consuming 50% of the total quota, so that
  // two directories must be removed to meet quota.

  // Create least-recently-used directories & their contents.
  const base::FilePath temp_path = temp_dir_.GetPath();
  CreateDummyCdmDirectory(temp_path, kOriginDirectory1, kKeySystemDirectory1,
                          kTestQuotaBytes / 2);
  CreateDummyCdmDirectory(temp_path, kOriginDirectory2, kKeySystemDirectory2,
                          kTestQuotaBytes / 2);
  CreateDummyCdmDirectory(temp_path, kOriginDirectory1,
                          kEmptyKeySystemDirectory, 0);

  // Sleep to account for coarse-grained filesystem timestamps.
  base::PlatformThread::Sleep(base::Seconds(1));

  // Create the recently-used directories.
  CreateDummyCdmDirectory(temp_path, kOriginDirectory1, kKeySystemDirectory2,
                          kTestQuotaBytes / 2);
  CreateDummyCdmDirectory(temp_path, kOriginDirectory2, kKeySystemDirectory1,
                          kTestQuotaBytes / 2);
  CreateDummyCdmDirectory(temp_path, kOriginDirectory2,
                          kEmptyKeySystemDirectory, 0);

  // Create the CDM manager, to run the data directory quota enforcement.
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({}, kTestQuotaBytes);

  // Use a CreateAndProvision() request as a proxy to wait for quota enforcement
  // to finish being applied.
  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_ptr.set_error_handler([&](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_NOT_FOUND);
    run_loop.Quit();
  });

  cdm_manager->CreateAndProvision(
      "com.key_system", url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(
      temp_path.Append(kOriginDirectory1).Append(kKeySystemDirectory1)));
  EXPECT_FALSE(base::PathExists(
      temp_path.Append(kOriginDirectory2).Append(kKeySystemDirectory2)));

  EXPECT_TRUE(base::PathExists(
      temp_path.Append(kOriginDirectory1).Append(kKeySystemDirectory2)));
  EXPECT_TRUE(base::PathExists(
      temp_path.Append(kOriginDirectory2).Append(kKeySystemDirectory1)));

  // Empty directories are currently always treated as old, causing them all to
  // be deleted if the CDM data directory exceeds its quota.
  EXPECT_FALSE(base::PathExists(
      temp_path.Append(kOriginDirectory1).Append(kEmptyKeySystemDirectory)));
  EXPECT_FALSE(base::PathExists(
      temp_path.Append(kOriginDirectory2).Append(kEmptyKeySystemDirectory)));
}

// Verify that if all key-system sub-directories for a given origin have been
// deleted then the origin's directory is also deleted.
TEST_F(FuchsiaCdmManagerTest, EmptyOriginDirectory) {
  constexpr uint64_t kTestQuotaBytes = 1024;
  constexpr char kInactiveOriginDirectory[] = "origin1";
  constexpr char kActiveOriginDirectory[] = "origin2";
  constexpr char kKeySystemDirectory1[] = "key_system1";
  constexpr char kKeySystemDirectory2[] = "key_system2";

  // Create dummy data for an inactive origin.
  const base::FilePath temp_path = temp_dir_.GetPath();
  CreateDummyCdmDirectory(temp_path, kInactiveOriginDirectory,
                          kKeySystemDirectory1, kTestQuotaBytes / 2);
  CreateDummyCdmDirectory(temp_path, kInactiveOriginDirectory,
                          kKeySystemDirectory2, kTestQuotaBytes / 2);

  // Sleep to account for coarse-grained filesystem timestamps.
  base::PlatformThread::Sleep(base::Seconds(1));

  // Create dummy data for a recently-used, active origin.
  CreateDummyCdmDirectory(temp_path, kActiveOriginDirectory,
                          kKeySystemDirectory2, kTestQuotaBytes);

  // Create the CDM manager, to run the data directory quota enforcement.
  std::unique_ptr<FuchsiaCdmManager> cdm_manager =
      CreateFuchsiaCdmManager({}, kTestQuotaBytes);

  // Use a CreateAndProvision() request as a proxy to wait for quota enforcement
  // to finish being applied.
  base::RunLoop run_loop;
  drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_ptr.set_error_handler([&](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_NOT_FOUND);
    run_loop.Quit();
  });

  cdm_manager->CreateAndProvision(
      "com.key_system", url::Origin(),
      base::BindRepeating(&CreateMockProvisionFetcher), cdm_ptr.NewRequest());
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(temp_path.Append(kInactiveOriginDirectory)));
  EXPECT_TRUE(base::PathExists(
      temp_path.Append(kActiveOriginDirectory).Append(kKeySystemDirectory2)));
}

}  // namespace
}  // namespace media
