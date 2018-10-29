// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
#include "content/common/service_worker/service_worker_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

typedef ServiceWorkerDatabase::RegistrationData RegistrationData;
typedef ServiceWorkerDatabase::ResourceRecord Resource;

struct AvailableIds {
  int64_t reg_id;
  int64_t res_id;
  int64_t ver_id;

  AvailableIds() : reg_id(-1), res_id(-1), ver_id(-1) {}
  ~AvailableIds() {}
};

GURL URL(const GURL& origin, const std::string& path) {
  EXPECT_TRUE(origin.is_valid());
  EXPECT_EQ(origin, origin.GetOrigin());
  GURL out(origin.spec() + path);
  EXPECT_TRUE(out.is_valid());
  return out;
}

Resource CreateResource(int64_t resource_id,
                        const GURL& url,
                        uint64_t size_bytes) {
  EXPECT_TRUE(url.is_valid());
  return Resource(resource_id, url, size_bytes);
}

ServiceWorkerDatabase* CreateDatabase(const base::FilePath& path) {
  return new ServiceWorkerDatabase(path);
}

ServiceWorkerDatabase* CreateDatabaseInMemory() {
  return new ServiceWorkerDatabase(base::FilePath());
}

void VerifyRegistrationData(const RegistrationData& expected,
                            const RegistrationData& actual) {
  EXPECT_EQ(expected.registration_id, actual.registration_id);
  EXPECT_EQ(expected.scope, actual.scope);
  EXPECT_EQ(expected.script, actual.script);
  EXPECT_EQ(expected.script_type, actual.script_type);
  EXPECT_EQ(expected.update_via_cache, actual.update_via_cache);
  EXPECT_EQ(expected.version_id, actual.version_id);
  EXPECT_EQ(expected.is_active, actual.is_active);
  EXPECT_EQ(expected.has_fetch_handler, actual.has_fetch_handler);
  EXPECT_EQ(expected.last_update_check, actual.last_update_check);
  EXPECT_EQ(expected.used_features, actual.used_features);
  EXPECT_EQ(expected.resources_total_size_bytes,
            actual.resources_total_size_bytes);
}

void VerifyResourceRecords(const std::vector<Resource>& expected,
                           const std::vector<Resource>& actual) {
  ASSERT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i].resource_id, actual[i].resource_id);
    EXPECT_EQ(expected[i].url, actual[i].url);
    EXPECT_EQ(expected[i].size_bytes, actual[i].size_bytes);
  }
}

}  // namespace

TEST(ServiceWorkerDatabaseTest, OpenDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  // Should be false because the database does not exist at the path.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->LazyOpen(false));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));

  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, OpenDatabase_InMemory) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Should be false because the database does not exist in memory.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->LazyOpen(false));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));
  database.reset(CreateDatabaseInMemory());

  // Should be false because the database is not persistent.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_ValidSchemaVersion) {
  GURL origin("https://example.com");
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));

  // Opening a new database does not write anything, so its schema version
  // should be 0.
  int64_t db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadDatabaseVersion(&db_version));
  EXPECT_EQ(0u, db_version);

  // First writing triggers database initialization and bumps the schema
  // version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(
                data, resources, &deleted_version, &newly_purgeable_resources));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadDatabaseVersion(&db_version));
  EXPECT_LT(0, db_version);
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_ObsoleteSchemaVersion) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));

  // First writing triggers database initialization and bumps the schema
  // version.
  GURL origin("https://example.com");
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));
  int64_t db_version = -1;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_LT(0, db_version);

  // Emulate an obsolete schema version.
  int64_t old_db_version = 1;
  leveldb::WriteBatch batch;
  batch.Put("INITDATA_DB_VERSION", base::Int64ToString(old_db_version));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK, database->WriteBatch(&batch));
  db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_EQ(old_db_version, db_version);

  // Opening the database whose schema version is obsolete should fail.
  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_FAILED,
            database->LazyOpen(true));
}

TEST(ServiceWorkerDatabaseTest, DatabaseVersion_CorruptedSchemaVersion) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));

  // First writing triggers database initialization and bumps the schema
  // version.
  GURL origin("https://example.com");
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));
  int64_t db_version = -1;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadDatabaseVersion(&db_version));
  ASSERT_LT(0, db_version);

  // Emulate a corrupted schema version.
  int64_t corrupted_db_version = -10;
  leveldb::WriteBatch batch;
  batch.Put("INITDATA_DB_VERSION", base::Int64ToString(corrupted_db_version));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK, database->WriteBatch(&batch));
  db_version = -1;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED,
            database->ReadDatabaseVersion(&db_version));

  // Opening the database whose schema version is corrupted should fail.
  database.reset(CreateDatabase(database_dir.GetPath()));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED,
            database->LazyOpen(true));
}

TEST(ServiceWorkerDatabaseTest, GetNextAvailableIds) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  GURL origin("https://example.com");

  // The database has never been used, so returns initial values.
  AvailableIds ids;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  // Writing uncommitted resources bumps the next available resource id.
  const int64_t kUncommittedIds[] = {0, 1, 3, 5, 6, 10};
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteUncommittedResourceIds(std::set<int64_t>(
          kUncommittedIds, kUncommittedIds + arraysize(kUncommittedIds))));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(11, ids.res_id);

  // Writing purgeable resources bumps the next available id.
  const int64_t kPurgeableIds[] = {4, 12, 16, 17, 20};
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUncommittedResourceIds(std::set<int64_t>(
                kPurgeableIds, kPurgeableIds + arraysize(kPurgeableIds))));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);

  // Writing a registration bumps the next available registration and version
  // ids.
  std::vector<Resource> resources1;
  RegistrationData data1;
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  data1.registration_id = 100;
  data1.scope = URL(origin, "/foo");
  data1.script = URL(origin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 300;
  resources1.push_back(CreateResource(1, data1.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));

  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->GetNextAvailableIds(&ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);

  // Writing a registration whose ids are lower than the stored ones should not
  // bump the next available ids.
  RegistrationData data2;
  data2.registration_id = 10;
  data2.scope = URL(origin, "/bar");
  data2.script = URL(origin, "/script2.js");
  data2.version_id = 20;
  data2.resources_total_size_bytes = 400;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Same with resources.
  int64_t kLowResourceId = 15;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUncommittedResourceIds(
                std::set<int64_t>(&kLowResourceId, &kLowResourceId + 1)));

  // Close and reopen the database to verify the stored values.
  database.reset(CreateDatabase(database_dir.GetPath()));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(21, ids.res_id);
}

TEST(ServiceWorkerDatabaseTest, GetOriginsWithRegistrations) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  std::set<GURL> origins;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetOriginsWithRegistrations(&origins));
  EXPECT_TRUE(origins.empty());

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  GURL origin1("https://example.com");
  RegistrationData data1;
  data1.registration_id = 123;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 456;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));

  GURL origin2("https://www.example.com");
  RegistrationData data2;
  data2.registration_id = 234;
  data2.scope = URL(origin2, "/bar");
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 567;
  data2.resources_total_size_bytes = 200;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  GURL origin3("https://example.org");
  RegistrationData data3;
  data3.registration_id = 345;
  data3.scope = URL(origin3, "/hoge");
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 678;
  data3.resources_total_size_bytes = 300;
  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data3, resources3, &deleted_version,
                                        &newly_purgeable_resources));

  // |origin3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 456;
  data4.scope = URL(origin3, "/fuga");
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 789;
  data4.resources_total_size_bytes = 400;
  std::vector<Resource> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data4, resources4, &deleted_version,
                                        &newly_purgeable_resources));

  origins.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(base::ContainsKey(origins, origin1));
  EXPECT_TRUE(base::ContainsKey(origins, origin2));
  EXPECT_TRUE(base::ContainsKey(origins, origin3));

  // |origin3| has another registration, so should not remove it from the
  // unique origin list.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data4.registration_id,
                                         origin3,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data4.registration_id, deleted_version.registration_id);

  origins.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(base::ContainsKey(origins, origin1));
  EXPECT_TRUE(base::ContainsKey(origins, origin2));
  EXPECT_TRUE(base::ContainsKey(origins, origin3));

  // |origin3| should be removed from the unique origin list.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data3.registration_id,
                                         origin3,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data3.registration_id, deleted_version.registration_id);

  origins.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(base::ContainsKey(origins, origin1));
  EXPECT_TRUE(base::ContainsKey(origins, origin2));
}

TEST(ServiceWorkerDatabaseTest, GetRegistrationsForOrigin) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin1("https://example.com");
  GURL origin2("https://www.example.com");
  GURL origin3("https://example.org");

  std::vector<RegistrationData> registrations;
  std::vector<std::vector<Resource>> resources_list;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetRegistrationsForOrigin(origin1, &registrations,
                                                &resources_list));
  EXPECT_TRUE(registrations.empty());
  EXPECT_TRUE(resources_list.empty());

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 1000;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetRegistrationsForOrigin(origin1, &registrations,
                                                &resources_list));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data1, registrations[0]);
  EXPECT_EQ(1U, resources_list.size());
  VerifyResourceRecords(resources1, resources_list[0]);

  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 2000;
  data2.resources_total_size_bytes = 200;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetRegistrationsForOrigin(origin2, &registrations,
                                                &resources_list));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data2, registrations[0]);
  EXPECT_EQ(1U, resources_list.size());
  VerifyResourceRecords(resources2, resources_list[0]);

  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/hoge");
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 3000;
  data3.resources_total_size_bytes = 300;
  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data3, resources3, &deleted_version,
                                        &newly_purgeable_resources));

  // |origin3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 400;
  data4.scope = URL(origin3, "/fuga");
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 4000;
  data4.resources_total_size_bytes = 400;
  std::vector<Resource> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data4, resources4, &deleted_version,
                                        &newly_purgeable_resources));

  registrations.clear();
  resources_list.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetRegistrationsForOrigin(origin3, &registrations,
                                                &resources_list));
  EXPECT_EQ(2U, registrations.size());
  VerifyRegistrationData(data3, registrations[0]);
  VerifyRegistrationData(data4, registrations[1]);
  EXPECT_EQ(2U, resources_list.size());
  VerifyResourceRecords(resources3, resources_list[0]);
  VerifyResourceRecords(resources4, resources_list[1]);

  // The third parameter |opt_resources_list| to GetRegistrationsForOrigin()
  // is optional. So, nullptr should be acceptable.
  registrations.clear();
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->GetRegistrationsForOrigin(origin1, &registrations, nullptr));
  EXPECT_EQ(1U, registrations.size());
  VerifyRegistrationData(data1, registrations[0]);
}

TEST(ServiceWorkerDatabaseTest, GetAllRegistrations) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  std::vector<RegistrationData> registrations;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetAllRegistrations(&registrations));
  EXPECT_TRUE(registrations.empty());

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  GURL origin1("https://www1.example.com");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 1000;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));

  GURL origin2("https://www2.example.com");
  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.script = URL(origin2, "/script2.js");
  data2.version_id = 2000;
  data2.resources_total_size_bytes = 200;
  data2.update_via_cache = blink::mojom::ServiceWorkerUpdateViaCache::kNone;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  GURL origin3("https://www3.example.com");
  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/hoge");
  data3.script = URL(origin3, "/script3.js");
  data3.version_id = 3000;
  data3.resources_total_size_bytes = 300;
  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(3, data3.script, 300));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data3, resources3, &deleted_version,
                                        &newly_purgeable_resources));

  // |origin3| has two registrations.
  RegistrationData data4;
  data4.registration_id = 400;
  data4.scope = URL(origin3, "/fuga");
  data4.script = URL(origin3, "/script4.js");
  data4.version_id = 4000;
  data4.resources_total_size_bytes = 400;
  std::vector<Resource> resources4;
  resources4.push_back(CreateResource(4, data4.script, 400));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data4, resources4, &deleted_version,
                                        &newly_purgeable_resources));

  registrations.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetAllRegistrations(&registrations));
  EXPECT_EQ(4U, registrations.size());

  VerifyRegistrationData(data1, registrations[0]);
  VerifyRegistrationData(data2, registrations[1]);
  VerifyRegistrationData(data3, registrations[2]);
  VerifyRegistrationData(data4, registrations[3]);
}

TEST(ServiceWorkerDatabaseTest, Registration_Basic) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 10939 + 200;
  data.used_features = {124, 901, 1019};

  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource1"), 10939));
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 200));

  // Write a resource to the uncommitted list to make sure that writing
  // registration removes resource ids associated with the registration from
  // the uncommitted list.
  std::set<int64_t> uncommitted_ids;
  uncommitted_ids.insert(resources[0].resource_id);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUncommittedResourceIds(uncommitted_ids));
  std::set<int64_t> uncommitted_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_EQ(uncommitted_ids, uncommitted_ids_out);

  ServiceWorkerDatabase::RegistrationData deleted_version;
  deleted_version.version_id = 222;  // Dummy initial value
  std::vector<int64_t> newly_purgeable_resources;

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(
                data, resources, &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());

  // Make sure that the registration and resource records are stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  VerifyResourceRecords(resources, resources_out);
  GURL origin_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadRegistrationOrigin(data.registration_id, &origin_out));
  EXPECT_EQ(origin, origin_out);

  // Make sure that the resource is removed from the uncommitted list.
  uncommitted_ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_TRUE(uncommitted_ids_out.empty());

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data.registration_id,
                                         origin,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data.version_id, deleted_version.version_id);
  ASSERT_EQ(resources.size(), newly_purgeable_resources.size());
  for (size_t i = 0; i < resources.size(); ++i)
    EXPECT_EQ(newly_purgeable_resources[i], resources[i].resource_id);

  // Make sure that the registration and resource records are gone.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadRegistrationOrigin(data.registration_id, &origin_out));

  // Resources should be purgeable because these are no longer referred.
  std::set<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources[0].resource_id));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources[1].resource_id));
}

TEST(ServiceWorkerDatabaseTest, DeleteNonExistentRegistration) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 19 + 29129;

  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, URL(origin, "/resource1"), 19));
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 29129));

  const int64_t kNonExistentRegistrationId = 999;
  const int64_t kArbitraryVersionId = 222;  // Used as a dummy initial value

  ServiceWorkerDatabase::RegistrationData deleted_version;
  deleted_version.version_id = kArbitraryVersionId;
  std::vector<int64_t> newly_purgeable_resources;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(
                data, resources, &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());

  // Delete from an origin that has a registration.
  deleted_version.version_id = kArbitraryVersionId;
  newly_purgeable_resources.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(kNonExistentRegistrationId,
                                         origin,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());

  // Delete from an origin that has no registration.
  deleted_version.version_id = kArbitraryVersionId;
  newly_purgeable_resources.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(
                kNonExistentRegistrationId, GURL("https://example.net"),
                &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());
}

TEST(ServiceWorkerDatabaseTest, Registration_Overwrite) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.com");
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/resource1");
  data.version_id = 200;
  data.resources_total_size_bytes = 10 + 11;
  data.used_features = {124, 901, 1019};

  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, URL(origin, "/resource1"), 10));
  resources1.push_back(CreateResource(2, URL(origin, "/resource2"), 11));

  ServiceWorkerDatabase::RegistrationData deleted_version;
  deleted_version.version_id = 222;  // Dummy inital value
  std::vector<int64_t> newly_purgeable_resources;

  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data, resources1, &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());

  // Make sure that the registration and resource records are stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  VerifyResourceRecords(resources1, resources_out);

  // Update the registration.
  RegistrationData updated_data = data;
  updated_data.script = URL(origin, "/resource3");
  updated_data.version_id = data.version_id + 1;
  updated_data.resources_total_size_bytes = 12 + 13;
  updated_data.used_features = {109, 421, 9101};
  updated_data.script_type = blink::mojom::ScriptType::kModule;
  updated_data.update_via_cache =
      blink::mojom::ServiceWorkerUpdateViaCache::kAll;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(3, URL(origin, "/resource3"), 12));
  resources2.push_back(CreateResource(4, URL(origin, "/resource4"), 13));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(updated_data,
                                        resources2,
                                        &deleted_version,
                                        &newly_purgeable_resources));
  EXPECT_EQ(data.version_id, deleted_version.version_id);
  ASSERT_EQ(resources1.size(), newly_purgeable_resources.size());
  for (size_t i = 0; i < resources1.size(); ++i)
    EXPECT_EQ(newly_purgeable_resources[i], resources1[i].resource_id);

  // Make sure that |updated_data| is stored and resources referred from |data|
  // is moved to the purgeable list.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      updated_data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(updated_data, data_out);
  VerifyResourceRecords(resources2, resources_out);

  std::set<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources1[0].resource_id));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources1[1].resource_id));
}

TEST(ServiceWorkerDatabaseTest, Registration_Multiple) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  // Add registration1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin, "/foo");
  data1.script = URL(origin, "/resource1");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 1451 + 15234;

  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, URL(origin, "/resource1"), 1451));
  resources1.push_back(CreateResource(2, URL(origin, "/resource2"), 15234));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data1, resources1, &deleted_version, &newly_purgeable_resources));

  // Add registration2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(origin, "/bar");
  data2.script = URL(origin, "/resource3");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 5 + 6;

  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(3, URL(origin, "/resource3"), 5));
  resources2.push_back(CreateResource(4, URL(origin, "/resource4"), 6));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data2, resources2, &deleted_version, &newly_purgeable_resources));

  // Make sure that registration1 is stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      data1.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data1, data_out);
  VerifyResourceRecords(resources1, resources_out);
  GURL origin_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadRegistrationOrigin(data1.registration_id, &origin_out));
  EXPECT_EQ(origin, origin_out);

  // Make sure that registration2 is also stored.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      data2.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data2, data_out);
  VerifyResourceRecords(resources2, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadRegistrationOrigin(data2.registration_id, &origin_out));
  EXPECT_EQ(origin, origin_out);

  std::set<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_TRUE(purgeable_ids_out.empty());

  // Delete registration1.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data1.registration_id,
                                         origin,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data1.registration_id, deleted_version.registration_id);

  // Make sure that registration1 is gone.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistration(
                data1.registration_id, origin, &data_out, &resources_out));
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadRegistrationOrigin(data1.registration_id, &origin_out));

  purgeable_ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(2u, purgeable_ids_out.size());
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources1[0].resource_id));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, resources1[1].resource_id));

  // Make sure that registration2 is still alive.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      data2.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data2, data_out);
  VerifyResourceRecords(resources2, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadRegistrationOrigin(data2.registration_id, &origin_out));
  EXPECT_EQ(origin, origin_out);
}

TEST(ServiceWorkerDatabaseTest, Registration_UninitializedDatabase) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL origin("https://example.com");

  // Should be failed because the database does not exist.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistration(
                100, origin, &data_out, &resources_out));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerRegistrationId,
            data_out.registration_id);
  EXPECT_TRUE(resources_out.empty());
  GURL origin_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistrationOrigin(100, &origin_out));

  // Deleting non-existent registration should succeed.
  RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(
                100, origin, &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());

  // Actually create a new database, but not initialized yet.
  database->LazyOpen(true);

  // Should be failed because the database is not initialized.
  ASSERT_EQ(ServiceWorkerDatabase::UNINITIALIZED, database->state_);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistration(
                100, origin, &data_out, &resources_out));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerRegistrationId,
            data_out.registration_id);
  EXPECT_TRUE(resources_out.empty());
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadRegistrationOrigin(100, &origin_out));

  // Deleting non-existent registration should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(
                100, origin, &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId,
            deleted_version.version_id);
  EXPECT_TRUE(newly_purgeable_resources.empty());
}

TEST(ServiceWorkerDatabaseTest, Registration_ScriptType) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  // Default script type.
  GURL origin1("https://www1.example.com");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/resource1");
  data1.version_id = 100;
  data1.resources_total_size_bytes = 10 + 10000;
  EXPECT_EQ(blink::mojom::ScriptType::kClassic, data1.script_type);
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, URL(origin1, "/resource1"), 10));
  resources1.push_back(CreateResource(2, URL(origin1, "/resource2"), 10000));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));

  // Classic script type.
  GURL origin2("https://www2.example.com");
  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = URL(origin2, "/bar");
  data2.script = URL(origin2, "/resource3");
  data2.version_id = 200;
  data2.resources_total_size_bytes = 20 + 20000;
  data2.script_type = blink::mojom::ScriptType::kClassic;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(3, URL(origin2, "/resource3"), 20));
  resources2.push_back(CreateResource(4, URL(origin2, "/resource4"), 20000));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Module script type.
  GURL origin3("https://www3.example.com");
  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = URL(origin3, "/baz");
  data3.script = URL(origin3, "/resource5");
  data3.version_id = 300;
  data3.resources_total_size_bytes = 30 + 30000;
  data3.script_type = blink::mojom::ScriptType::kModule;
  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(5, URL(origin3, "/resource5"), 30));
  resources3.push_back(CreateResource(6, URL(origin3, "/resource6"), 30000));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data3, resources3, &deleted_version,
                                        &newly_purgeable_resources));

  RegistrationData data;
  std::vector<Resource> resources;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(data1.registration_id, origin1, &data,
                                       &resources));
  VerifyRegistrationData(data1, data);
  VerifyResourceRecords(resources1, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(data2.registration_id, origin2, &data,
                                       &resources));
  VerifyRegistrationData(data2, data);
  VerifyResourceRecords(resources2, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(data3.registration_id, origin3, &data,
                                       &resources));
  VerifyRegistrationData(data3, data);
  VerifyResourceRecords(resources3, resources);
  EXPECT_EQ(2U, resources.size());
  resources.clear();
}

TEST(ServiceWorkerDatabaseTest, UserData_Basic) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin, "/foo");
  data.script = URL(kOrigin, "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(
                data, resources, &deleted_version, &newly_purgeable_resources));

  // Write user data associated with the stored registration.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data.registration_id, kOrigin,
                                    {{"key1", "data"}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data", user_data_out[0]);

  // Writing user data not associated with the stored registration should be
  // failed.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->WriteUserData(300, kOrigin, {{"key1", "data"}}));

  // Write empty user data for a different key.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data.registration_id, kOrigin,
                                    {{"key2", std::string()}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ(std::string(), user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data", user_data_out[0]);

  // Overwrite the existing user data.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data.registration_id, kOrigin,
                                    {{"key1", "overwrite"}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("overwrite", user_data_out[0]);

  // Delete the user data.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserData(data.registration_id, {"key1"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_TRUE(user_data_out.empty());
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ(std::string(), user_data_out[0]);

  // Write/overwrite multiple user data keys.
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteUserData(
          data.registration_id, kOrigin,
          {{"key2", "overwrite2"}, {"key3", "data3"}, {"key4", "data4"}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserData(data.registration_id,
                                   {"key2", "key3", "key4"}, &user_data_out));
  ASSERT_EQ(3u, user_data_out.size());
  EXPECT_EQ("overwrite2", user_data_out[0]);
  EXPECT_EQ("data3", user_data_out[1]);
  EXPECT_EQ("data4", user_data_out[2]);
  // Multiple reads fail if one is not found.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadUserData(data.registration_id, {"key2", "key1"},
                                   &user_data_out));
  EXPECT_TRUE(user_data_out.empty());

  // Delete multiple user data keys, even if some are not found.
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->DeleteUserData(data.registration_id, {"key1", "key2", "key3"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data.registration_id, {"key3"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data.registration_id, {"key4"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data4", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest,
     UserData_ReadUserDataForAllRegistrationsByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin, "/foo");
  data1.script = URL(kOrigin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin, "/bar");
  data2.script = URL(kOrigin, "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with the registration1.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value1"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key_prefix:key2", "value2"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key_prefix:key3", "value3"}}));

  // Write user data associated with the registration2.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value1"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"key_prefix:key2", "value2"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"another_key_prefix:key1", "value1"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"another_key_prefix:key2", "value2"}}));

  // Get all registrations with user data by key prefix.
  std::vector<std::pair<int64_t, std::string>> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "key_prefix:", &user_data_list));
  ASSERT_EQ(5u, user_data_list.size());

  EXPECT_EQ(data1.registration_id, user_data_list[0].first);
  EXPECT_EQ("value1", user_data_list[0].second);
  EXPECT_EQ(data2.registration_id, user_data_list[1].first);
  EXPECT_EQ("value1", user_data_list[1].second);
  EXPECT_EQ(data1.registration_id, user_data_list[2].first);
  EXPECT_EQ("value2", user_data_list[2].second);
  EXPECT_EQ(data2.registration_id, user_data_list[3].first);
  EXPECT_EQ("value2", user_data_list[3].second);
  EXPECT_EQ(data1.registration_id, user_data_list[4].first);
  EXPECT_EQ("value3", user_data_list[4].second);
}

TEST(ServiceWorkerDatabaseTest, ReadUserDataByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin, "/foo");
  data.script = URL(kOrigin, "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with the registration.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value_c1"},
                                     {"key_prefix:key2", "value_c2"},
                                     {"other_key_prefix:k1", "value_d1"},
                                     {"other_key_prefix:k2", "value_d2"}}));

  std::vector<std::string> user_data;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "bogus_prefix:", &user_data));
  EXPECT_THAT(user_data, IsEmpty());

  user_data.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "key_prefix:", &user_data));
  EXPECT_THAT(user_data, ElementsAreArray({"value_c1", "value_c2"}));

  user_data.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataByKeyPrefix(data.registration_id,
                                              "other_key_prefix:", &user_data));
  EXPECT_THAT(user_data, ElementsAreArray({"value_d1", "value_d2"}));
}

TEST(ServiceWorkerDatabaseTest, ReadUserKeysAndDataByKeyPrefix) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(kOrigin, "/foo");
  data.script = URL(kOrigin, "/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 100;
  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with the registration.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value_c1"},
                                     {"key_prefix:key2", "value_c2"},
                                     {"other_key_prefix:k1", "value_d1"},
                                     {"other_key_prefix:k2", "value_d2"}}));

  base::flat_map<std::string, std::string> user_data_map;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "bogus_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map, IsEmpty());

  user_data_map.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "key_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map,
              ElementsAreArray(std::vector<std::pair<std::string, std::string>>{
                  {"key1", "value_c1"}, {"key2", "value_c2"}}));

  user_data_map.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserKeysAndDataByKeyPrefix(
                data.registration_id, "other_key_prefix:", &user_data_map));
  EXPECT_THAT(user_data_map,
              ElementsAreArray(std::vector<std::pair<std::string, std::string>>{
                  {"k1", "value_d1"}, {"k2", "value_d2"}}));
}

TEST(ServiceWorkerDatabaseTest, UserData_DeleteUserDataByKeyPrefixes) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin, "/foo");
  data1.script = URL(kOrigin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin, "/bar");
  data2.script = URL(kOrigin, "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with registration 1.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value_a1"},
                                     {"key_prefix:key2", "value_a2"},
                                     {"key_prefix:key3", "value_a3"},
                                     {"kept_key_prefix:key1", "value_b1"}}));

  // Write user data associated with registration 2.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"key_prefix:key1", "value_c1"},
                                     {"key_prefix:key2", "value_c2"},
                                     {"other_key_prefix:key1", "value_d1"},
                                     {"other_key_prefix:key2", "value_d2"},
                                     {"kept_key_prefix:key1", "value_e1"},
                                     {"kept_key_prefix:key2", "value_e2"}}));

  // Deleting user data by key prefixes should return STATUS_OK (rather than
  // STATUS_ERROR_NOT_FOUND) even if no keys match the prefixes and so nothing
  // is deleted.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserDataByKeyPrefixes(
                data2.registration_id,
                {"not_found_key_prefix1:", "not_found_key_prefix2:"}));

  // Actually delete user data by key prefixes for registration 2.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserDataByKeyPrefixes(
                data2.registration_id,
                {"key_prefix:", "other_key_prefix:", "not_found_key_prefix:"}));

  // User data with deleted "key_prefix:" should only remain for registration 1.
  std::vector<std::pair<int64_t, std::string>> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "key_prefix:", &user_data_list));
  ASSERT_EQ(3u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0].first);
  EXPECT_EQ("value_a1", user_data_list[0].second);
  EXPECT_EQ(data1.registration_id, user_data_list[1].first);
  EXPECT_EQ("value_a2", user_data_list[1].second);
  EXPECT_EQ(data1.registration_id, user_data_list[2].first);
  EXPECT_EQ("value_a3", user_data_list[2].second);

  // User data for second deleted key prefix should also have been deleted.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "other_key_prefix:", &user_data_list));
  ASSERT_EQ(0u, user_data_list.size());

  // User data with "kept_key_prefix:" that was not deleted should remain on
  // both registrations.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrationsByKeyPrefix(
                "kept_key_prefix:", &user_data_list));
  ASSERT_EQ(3u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0].first);
  EXPECT_EQ("value_b1", user_data_list[0].second);
  EXPECT_EQ(data2.registration_id, user_data_list[1].first);
  EXPECT_EQ("value_e1", user_data_list[1].second);
  EXPECT_EQ(data2.registration_id, user_data_list[2].first);
  EXPECT_EQ("value_e2", user_data_list[2].second);
}

TEST(ServiceWorkerDatabaseTest, UserData_DataIsolation) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin, "/foo");
  data1.script = URL(kOrigin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin, "/bar");
  data2.script = URL(kOrigin, "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  data2.update_via_cache = blink::mojom::ServiceWorkerUpdateViaCache::kImports;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with the registration1.
  std::vector<std::string> user_data_out;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key", "data1"}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));

  // Write user data associated with the registration2. This shouldn't overwrite
  // the data associated with registration1.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"key", "data2"}}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data2", user_data_out[0]);

  // Get all registrations with user data.
  std::vector<std::pair<int64_t, std::string>> user_data_list;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrations("key", &user_data_list));
  EXPECT_EQ(2u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0].first);
  EXPECT_EQ("data1", user_data_list[0].second);
  EXPECT_EQ(data2.registration_id, user_data_list[1].first);
  EXPECT_EQ("data2", user_data_list[1].second);

  // Delete the data associated with the registration2. This shouldn't delete
  // the data associated with registration1.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserData(data2.registration_id, {"key"}));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data1.registration_id, {"key"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data1", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data2.registration_id, {"key"}, &user_data_out));

  // And again get all registrations with user data.
  user_data_list.clear();
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadUserDataForAllRegistrations("key", &user_data_list));
  EXPECT_EQ(1u, user_data_list.size());
  EXPECT_EQ(data1.registration_id, user_data_list[0].first);
  EXPECT_EQ("data1", user_data_list[0].second);
}

TEST(ServiceWorkerDatabaseTest, UserData_DeleteRegistration) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Add registration 1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = URL(kOrigin, "/foo");
  data1.script = URL(kOrigin, "/script1.js");
  data1.version_id = 200;
  data1.resources_total_size_bytes = 100;
  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, data1.script, 100));

  // Add registration 2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = URL(kOrigin, "/bar");
  data2.script = URL(kOrigin, "/script2.js");
  data2.version_id = 201;
  data2.resources_total_size_bytes = 200;
  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(2, data2.script, 200));

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data1, resources1, &deleted_version,
                                        &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data2, resources2, &deleted_version,
                                        &newly_purgeable_resources));

  // Write user data associated with the registration1.
  std::vector<std::string> user_data_out;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key1", "data1"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, kOrigin,
                                    {{"key2", "data2"}}));
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("data1", user_data_out[0]);
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("data2", user_data_out[0]);

  // Write user data associated with the registration2.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, kOrigin,
                                    {{"key3", "data3"}}));
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  ASSERT_EQ("data3", user_data_out[0]);

  // Delete all data associated with the registration1. This shouldn't delete
  // the data associated with registration2.
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(
                data1.registration_id, kOrigin,
                &deleted_version, &newly_purgeable_resources));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data3", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest, UserData_UninitializedDatabase) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  const GURL kOrigin("https://example.com");

  // Should be failed because the database does not exist.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadUserData(100, {"key"}, &user_data_out));

  // Should be failed because the associated registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->WriteUserData(100, kOrigin, {{"key", "data"}}));

  // Deleting non-existent entry should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserData(100, {"key"}));

  // Actually create a new database, but not initialized yet.
  database->LazyOpen(true);

  // Should be failed because the database is not initialized.
  ASSERT_EQ(ServiceWorkerDatabase::UNINITIALIZED, database->state_);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadUserData(100, {"key"}, &user_data_out));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->WriteUserData(100, kOrigin, {{"key", "data"}}));

  // Deleting non-existent entry should succeed.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteUserData(100, {"key"}));
}

TEST(ServiceWorkerDatabaseTest, UpdateVersionToActive) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  // Should be false because a registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->UpdateVersionToActive(0, origin));

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/script.js");
  data.version_id = 200;
  data.is_active = false;
  data.resources_total_size_bytes = 100;
  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));

  // Make sure that the registration is stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Activate the registration.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->UpdateVersionToActive(data.registration_id, origin));

  // Make sure that the registration is activated.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  RegistrationData expected_data = data;
  expected_data.is_active = true;
  VerifyRegistrationData(expected_data, data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Delete the registration.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data.registration_id,
                                         origin,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data.registration_id, deleted_version.registration_id);

  // Should be false because the registration is gone.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->UpdateVersionToActive(data.registration_id, origin));
}

TEST(ServiceWorkerDatabaseTest, UpdateLastCheckTime) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  GURL origin("https://example.com");
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  // Should be false because a registration does not exist.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->UpdateLastCheckTime(0, origin, base::Time::Now()));

  // Add a registration.
  RegistrationData data;
  data.registration_id = 100;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/script.js");
  data.version_id = 200;
  data.last_update_check = base::Time::Now();
  data.resources_total_size_bytes = 100;
  std::vector<Resource> resources;
  resources.push_back(CreateResource(1, data.script, 100));
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));

  // Make sure that the registration is stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Update the last check time.
  base::Time updated_time = base::Time::Now();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->UpdateLastCheckTime(
                data.registration_id, origin, updated_time));

  // Make sure that the registration is updated.
  resources_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ReadRegistration(
                data.registration_id, origin, &data_out, &resources_out));
  RegistrationData expected_data = data;
  expected_data.last_update_check = updated_time;
  VerifyRegistrationData(expected_data, data_out);
  EXPECT_EQ(1u, resources_out.size());

  // Delete the registration.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteRegistration(data.registration_id,
                                         origin,
                                         &deleted_version,
                                         &newly_purgeable_resources));
  EXPECT_EQ(data.registration_id, deleted_version.registration_id);

  // Should be false because the registration is gone.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
            database->UpdateLastCheckTime(
                data.registration_id, origin, base::Time::Now()));
}

TEST(ServiceWorkerDatabaseTest, UncommittedAndPurgeableResourceIds) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Write {1, 2, 3} into the uncommitted list.
  std::set<int64_t> ids1;
  ids1.insert(1);
  ids1.insert(2);
  ids1.insert(3);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUncommittedResourceIds(ids1));

  std::set<int64_t> ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetUncommittedResourceIds(&ids_out));
  EXPECT_EQ(ids1, ids_out);

  // Write {2, 4} into the uncommitted list.
  std::set<int64_t> ids2;
  ids2.insert(2);
  ids2.insert(4);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUncommittedResourceIds(ids2));

  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetUncommittedResourceIds(&ids_out));
  std::set<int64_t> expected = base::STLSetUnion<std::set<int64_t>>(ids1, ids2);
  EXPECT_EQ(expected, ids_out);

  // Move {2, 4} from the uncommitted list to the purgeable list.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->PurgeUncommittedResourceIds(ids2));
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&ids_out));
  EXPECT_EQ(ids2, ids_out);

  // Delete {2, 4} from the purgeable list.
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->ClearPurgeableResourceIds(ids2));
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&ids_out));
  EXPECT_TRUE(ids_out.empty());

  // {1, 3} should be still in the uncommitted list.
  ids_out.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetUncommittedResourceIds(&ids_out));
  expected = base::STLSetDifference<std::set<int64_t>>(ids1, ids2);
  EXPECT_EQ(expected, ids_out);
}

TEST(ServiceWorkerDatabaseTest, DeleteAllDataForOrigin) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  // Data associated with |origin1| will be removed.
  GURL origin1("https://example.com");
  GURL origin2("https://example.org");

  // |origin1| has two registrations (registration1 and registration2).
  RegistrationData data1;
  data1.registration_id = 10;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/resource1");
  data1.version_id = 100;
  data1.resources_total_size_bytes = 2013 + 512;

  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, URL(origin1, "/resource1"), 2013));
  resources1.push_back(CreateResource(2, URL(origin1, "/resource2"), 512));
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data1, resources1, &deleted_version, &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, origin1,
                                    {{"key1", "data1"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data1.registration_id, origin1,
                                    {{"key2", "data2"}}));

  RegistrationData data2;
  data2.registration_id = 11;
  data2.scope = URL(origin1, "/bar");
  data2.script = URL(origin1, "/resource3");
  data2.version_id = 101;
  data2.resources_total_size_bytes = 4 + 5;

  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(3, URL(origin1, "/resource3"), 4));
  resources2.push_back(CreateResource(4, URL(origin1, "/resource4"), 5));
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data2, resources2, &deleted_version, &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, origin1,
                                    {{"key3", "data3"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data2.registration_id, origin1,
                                    {{"key4", "data4"}}));

  // |origin2| has one registration (registration3).
  RegistrationData data3;
  data3.registration_id = 12;
  data3.scope = URL(origin2, "/hoge");
  data3.script = URL(origin2, "/resource5");
  data3.version_id = 102;
  data3.resources_total_size_bytes = 6 + 7;

  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(5, URL(origin2, "/resource5"), 6));
  resources3.push_back(CreateResource(6, URL(origin2, "/resource6"), 7));
  ASSERT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->WriteRegistration(
          data3, resources3, &deleted_version, &newly_purgeable_resources));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data3.registration_id, origin2,
                                    {{"key5", "data5"}}));
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteUserData(data3.registration_id, origin2,
                                    {{"key6", "data6"}}));

  std::set<GURL> origins_to_delete;
  origins_to_delete.insert(origin1);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->DeleteAllDataForOrigins(origins_to_delete,
                                              &newly_purgeable_resources));

  // |origin1| should be removed from the unique origin list.
  std::set<GURL> unique_origins;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetOriginsWithRegistrations(&unique_origins));
  EXPECT_EQ(1u, unique_origins.size());
  EXPECT_TRUE(base::ContainsKey(unique_origins, origin2));

  // The registrations for |origin1| should be removed.
  std::vector<RegistrationData> registrations;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->GetRegistrationsForOrigin(origin1, &registrations, nullptr));
  EXPECT_TRUE(registrations.empty());
  GURL origin_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadRegistrationOrigin(data1.registration_id, &origin_out));

  // The registration for |origin2| should not be removed.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->ReadRegistration(
      data3.registration_id, origin2, &data_out, &resources_out));
  VerifyRegistrationData(data3, data_out);
  VerifyResourceRecords(resources3, resources_out);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadRegistrationOrigin(data3.registration_id, &origin_out));
  EXPECT_EQ(origin2, origin_out);

  // The resources associated with |origin1| should be purgeable.
  std::set<int64_t> purgeable_ids_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(4u, purgeable_ids_out.size());
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, 1));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, 2));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, 3));
  EXPECT_TRUE(base::ContainsKey(purgeable_ids_out, 4));

  // The user data associated with |origin1| should be removed.
  std::vector<std::string> user_data_out;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data1.registration_id, {"key1"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data1.registration_id, {"key2"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data2.registration_id, {"key3"}, &user_data_out));
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadUserData(data2.registration_id, {"key4"}, &user_data_out));

  // The user data associated with |origin2| should not be removed.
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data3.registration_id, {"key5"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data5", user_data_out[0]);
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      database->ReadUserData(data3.registration_id, {"key6"}, &user_data_out));
  ASSERT_EQ(1u, user_data_out.size());
  EXPECT_EQ("data6", user_data_out[0]);
}

TEST(ServiceWorkerDatabaseTest, DestroyDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.GetPath()));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->LazyOpen(true));
  ASSERT_TRUE(base::DirectoryExists(database_dir.GetPath()));

  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK, database->DestroyDatabase());
  ASSERT_FALSE(base::DirectoryExists(database_dir.GetPath()));
}

TEST(ServiceWorkerDatabaseTest, Corruption_NoMainResource) {
  std::unique_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;

  GURL origin("https://example.com");

  RegistrationData data;
  data.registration_id = 10;
  data.scope = URL(origin, "/foo");
  data.script = URL(origin, "/resource1");
  data.version_id = 100;
  data.resources_total_size_bytes = 2016;

  // Simulate that "/resource1" wasn't correctly written in the database by not
  // adding it.
  std::vector<Resource> resources;
  resources.push_back(CreateResource(2, URL(origin, "/resource2"), 2016));

  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));

  // The database should detect lack of the main resource (i.e. "/resource1").
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED,
            database->ReadRegistration(data.registration_id, origin, &data_out,
                                       &resources_out));
  EXPECT_TRUE(resources_out.empty());
}

}  // namespace content
