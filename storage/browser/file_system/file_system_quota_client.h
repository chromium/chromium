// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_

#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class FileSystemContext;

// All of the public methods of this class are called by the quota manager
// (except for the constructor/destructor).
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemQuotaClient
    : public QuotaClient {
 public:
  explicit FileSystemQuotaClient(FileSystemContext* file_system_context);

  // QuotaClient methods.
  void OnQuotaManagerDestroyed() override {}
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetOriginUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsForTypeCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeleteOriginDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  ~FileSystemQuotaClient() override;

  base::SequencedTaskRunner* file_task_runner() const;

  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_
