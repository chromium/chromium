// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_registrations_table.h"

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "content/public/browser/attribution_data_model.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

struct OsRegistrationData {
  std::string registration_origin;
  base::Time time;

  friend bool operator==(const OsRegistrationData&,
                         const OsRegistrationData&) = default;
};

class OsRegistrationsTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.CreateTable(&db_));

    // Prevent any rows from being deleted during the test by default.
    delegate_.set_delete_expired_os_registrations_frequency(
        base::TimeDelta::Max());
  }

 protected:
  std::vector<OsRegistrationData> GetOsRegistrationRows() {
    std::vector<OsRegistrationData> rows;

    static constexpr char kSelectSql[] =
        "SELECT registration_origin,time "
        "FROM os_registrations";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

    while (statement.Step()) {
      rows.emplace_back(/*registration_origin=*/statement.ColumnString(0),
                        /*time=*/statement.ColumnTime(1));
    }

    EXPECT_TRUE(statement.Succeeded());
    return rows;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_{sql::test::kTestTag};
  ConfigurableStorageDelegate delegate_;
  OsRegistrationsTable table_{&delegate_};
};

TEST_F(OsRegistrationsTableTest, AddOsRegistrations) {
  base::Time now = base::Time::Now();

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test")),
                             url::Origin::Create(GURL("https://b.test"))});

  EXPECT_THAT(
      GetOsRegistrationRows(),
      UnorderedElementsAre(
          OsRegistrationData(
              /*registration_origin=*/"https://a.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test", now)));
}

TEST_F(OsRegistrationsTableTest, ClearAllDataAllTime) {
  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test"))});

  task_environment_.FastForwardBy(base::Days(1));

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://b.test"))});

  ASSERT_THAT(GetOsRegistrationRows(), SizeIs(2));

  table_.ClearAllDataAllTime(&db_);
  EXPECT_THAT(GetOsRegistrationRows(), IsEmpty());
}

TEST_F(OsRegistrationsTableTest, ClearDataForOriginsInRange) {
  base::Time now = base::Time::Now();

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test")),
                             url::Origin::Create(GURL("https://b.test"))});

  task_environment_.FastForwardBy(base::Days(1));

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test")),
                             url::Origin::Create(GURL("https://b.test"))});

  ASSERT_THAT(GetOsRegistrationRows(), SizeIs(4));

  table_.ClearDataForOriginsInRange(
      &db_, /*delete_begin=*/now + base::Hours(1),
      /*delete_end=*/base::Time::Max(),
      /*filter=*/base::BindRepeating([](const blink::StorageKey& storage_key) {
        return storage_key ==
               blink::StorageKey::CreateFromStringForTesting("https://a.test");
      }));

  ASSERT_THAT(
      GetOsRegistrationRows(),
      UnorderedElementsAre(
          OsRegistrationData(/*registration_origin=*/"https://a.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test",
                             now + base::Days(1))));

  table_.ClearDataForOriginsInRange(&db_, /*delete_begin=*/now - base::Hours(1),
                                    /*delete_end=*/now,
                                    /*filter=*/base::NullCallback());

  EXPECT_THAT(
      GetOsRegistrationRows(),
      UnorderedElementsAre(OsRegistrationData(
          /*registration_origin=*/"https://b.test", now + base::Days(1))));
}

TEST_F(OsRegistrationsTableTest, DeleteExpiredRows) {
  delegate_.set_delete_expired_os_registrations_frequency(base::Hours(12));

  base::Time now = base::Time::Now();

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test"))});

  task_environment_.FastForwardBy(base::Hours(12));

  // No record has expired yet.
  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://b.test"))});

  ASSERT_THAT(GetOsRegistrationRows(), SizeIs(2));

  task_environment_.FastForwardBy(base::Days(45) - base::Hours(12));

  // This should delete the first record.
  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://c.test"))});

  EXPECT_THAT(GetOsRegistrationRows(),
              UnorderedElementsAre(
                  OsRegistrationData(/*registration_origin=*/"https://b.test",
                                     now + base::Hours(12)),
                  OsRegistrationData(/*registration_origin=*/"https://c.test",
                                     now + base::Days(45))));
}

TEST_F(OsRegistrationsTableTest, ClearDataForRegistrationOrigin) {
  base::Time now = base::Time::Now();

  auto origin_a = url::Origin::Create(GURL("https://a.test"));
  auto origin_b = url::Origin::Create(GURL("https://b.test"));

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test")),
                             url::Origin::Create(GURL("https://b.test"))});

  task_environment_.FastForwardBy(base::Days(1));

  table_.AddOsRegistrations(&db_,
                            {url::Origin::Create(GURL("https://a.test")),
                             url::Origin::Create(GURL("https://b.test"))});

  ASSERT_THAT(GetOsRegistrationRows(), SizeIs(4));

  table_.ClearDataForRegistrationOrigin(
      &db_, /*delete_begin=*/now + base::Hours(1),
      /*delete_end=*/base::Time::Max(),
      url::Origin::Create(GURL("https://a.test")));

  ASSERT_THAT(
      GetOsRegistrationRows(),
      UnorderedElementsAre(
          OsRegistrationData(/*registration_origin=*/"https://a.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test",
                             now + base::Days(1))));

  table_.ClearDataForRegistrationOrigin(
      &db_, /*delete_begin=*/now - base::Hours(1),
      /*delete_end=*/now, url::Origin::Create(GURL("https://b.test")));
  ASSERT_THAT(
      GetOsRegistrationRows(),
      UnorderedElementsAre(
          OsRegistrationData(/*registration_origin=*/"https://a.test", now),
          OsRegistrationData(/*registration_origin=*/"https://b.test",
                             now + base::Days(1))));
}

TEST_F(OsRegistrationsTableTest, GetOsRegistrationDataKeys) {
  url::Origin origin_1 = url::Origin::Create(GURL("https://a.r.test"));
  url::Origin origin_2 = url::Origin::Create(GURL("https://b.r.test"));
  url::Origin origin_3 = url::Origin::Create(GURL("https://c.test"));
  table_.AddOsRegistrations(&db_, {origin_1, origin_2, origin_3});

  std::set<AttributionDataModel::DataKey> keys;
  table_.AppendOsRegistrationDataKeys(&db_, keys);

  EXPECT_THAT(keys,
              UnorderedElementsAre(AttributionDataModel::DataKey(origin_1),
                                   AttributionDataModel::DataKey(origin_2),
                                   AttributionDataModel::DataKey(origin_3)));
}

}  // namespace
}  // namespace content
