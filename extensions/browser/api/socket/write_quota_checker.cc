// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/write_quota_checker.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

namespace {

// Maximum pending bytes to write.
constexpr size_t kWriteLimit = 200 * 1024 * 1024;

}  // namespace

WriteQuotaChecker::ScopedBytesLimitForTest::ScopedBytesLimitForTest(
    WriteQuotaChecker* checker,
    size_t new_bytes_limit)
    : scoped_bytes_limit_(&checker->bytes_limit_, new_bytes_limit) {}

WriteQuotaChecker::ScopedBytesLimitForTest::~ScopedBytesLimitForTest() =
    default;

// static
WriteQuotaChecker* WriteQuotaChecker::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<WriteQuotaChecker>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<WriteQuotaChecker>*
WriteQuotaChecker::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<WriteQuotaChecker>>
      factory;
  return factory.get();
}

WriteQuotaChecker::WriteQuotaChecker(content::BrowserContext* context)
    : bytes_limit_(kWriteLimit) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
}

WriteQuotaChecker::~WriteQuotaChecker() = default;

bool WriteQuotaChecker::TakeBytes(const ExtensionId& extension_id,
                                  size_t bytes) {
  auto& bytes_used = bytes_used_map_[extension_id];

  size_t new_bytes_used = bytes_used + bytes;
  if (new_bytes_used > bytes_limit_) {
    return false;
  }

  bytes_used = new_bytes_used;
  return true;
}

void WriteQuotaChecker::ReturnBytes(const ExtensionId& extension_id,
                                    size_t bytes) {
  auto it = bytes_used_map_.find(extension_id);
  CHECK(it != bytes_used_map_.end());
  CHECK_GE(it->second, bytes);

  it->second -= bytes;
  if (it->second == 0) {
    bytes_used_map_.erase(it);
  }
}

void WriteQuotaChecker::OnExtensionUnloaded(content::BrowserContext* context,
                                            const Extension* extension,
                                            UnloadedExtensionReason reason) {
  bytes_used_map_.erase(extension->id());
}

}  // namespace extensions
