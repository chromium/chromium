// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FACTORY_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FACTORY_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content::indexed_db {

class MockFactoryClient : public FactoryClient {
 public:
  MockFactoryClient();
  explicit MockFactoryClient(bool expect_connection);
  ~MockFactoryClient() override;

  MockFactoryClient(const MockFactoryClient&) = delete;
  MockFactoryClient& operator=(const MockFactoryClient&) = delete;

  void OnError(const DatabaseError& error) override;

  void OnDeleteSuccess(int64_t old_version) override;
  void OnOpenSuccess(std::unique_ptr<Connection> connection,
                     const blink::IndexedDBDatabaseMetadata& metadata) override;
  Connection* connection() { return connection_.get(); }

  std::unique_ptr<Connection> TakeConnection() {
    expect_connection_ = false;
    return std::move(connection_);
  }

  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<Connection> connection,
                       const blink::IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss_info) override;

  void CallOnUpgradeNeeded(base::OnceClosure closure);
  void CallOnDBSuccess(base::OnceClosure closure);
  void CallOnInfoSuccess(base::RepeatingClosure closure);

  bool error_called() { return error_called_; }
  bool upgrade_called() { return upgrade_called_; }
  bool info_called() { return info_called_; }

 protected:
  std::unique_ptr<Connection> connection_;

 private:
  bool expect_connection_ = true;
  bool error_called_ = false;
  bool upgrade_called_ = false;
  bool info_called_ = false;
  base::OnceClosure call_on_upgrade_needed_;
  base::OnceClosure call_on_db_success_;
  base::RepeatingClosure call_on_info_success_;
};

// This class wraps a `MockFactoryClient`, passing through all `FactoryClient`
// methods. This allows a test to create an underlying `MockFactoryClient` and
// pass ownership of a wrapper to the pending connection.
class ThunkFactoryClient : public MockFactoryClient {
 public:
  // `wrapped` must outlast this.
  explicit ThunkFactoryClient(FactoryClient& wrapped);
  ~ThunkFactoryClient() override = default;
  ThunkFactoryClient(const ThunkFactoryClient&) = delete;
  ThunkFactoryClient& operator=(const ThunkFactoryClient&) = delete;

  void OnError(const DatabaseError& error) override;
  void OnBlocked(int64_t existing_version) override;
  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<Connection> connection,
                       const blink::IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss_info) override;
  void OnOpenSuccess(std::unique_ptr<Connection> connection,
                     const blink::IndexedDBDatabaseMetadata& metadata) override;
  void OnDeleteSuccess(int64_t old_version) override;

 private:
  const raw_ref<FactoryClient> wrapped_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FACTORY_CLIENT_H_
