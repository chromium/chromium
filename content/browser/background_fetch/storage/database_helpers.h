// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_HELPERS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_HELPERS_H_

#include <string>

#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {
namespace background_fetch {

// The database schema is content/browser/background_fetch/storage/README.md.
// When making any changes to these keys or the related functions, you must
// update the README.md file as well.

// Warning: registration |developer_id|s may contain kSeparator characters.
const char kSeparator[] = "_";

const char kActiveRegistrationUniqueIdKeyPrefix[] =
    "bgfetch_active_registration_unique_id_";
const char kRegistrationKeyPrefix[] = "bgfetch_registration_";
const char kUIOptionsKeyPrefix[] = "bgfetch_ui_options_";
const char kPendingRequestKeyPrefix[] = "bgfetch_pending_request_";
const char kActiveRequestKeyPrefix[] = "bgfetch_active_request_";
const char kCompletedRequestKeyPrefix[] = "bgfetch_completed_request_";
const char kStorageVersionKeyPrefix[] = "bgfetch_storage_version_";

// Database Keys.
CONTENT_EXPORT std::string ActiveRegistrationUniqueIdKey(
    const std::string& developer_id);

CONTENT_EXPORT std::string RegistrationKey(const std::string& unique_id);

std::string UIOptionsKey(const std::string& unique_id);

std::string PendingRequestKeyPrefix(const std::string& unique_id);

std::string PendingRequestKey(const std::string& unique_id, int request_index);

std::string ActiveRequestKeyPrefix(const std::string& unique_id);

std::string ActiveRequestKey(const std::string& unique_id, int request_index);

std::string CompletedRequestKeyPrefix(const std::string& unique_id);

std::string CompletedRequestKey(const std::string& unique_id,
                                int request_index);

CONTENT_EXPORT std::string StorageVersionKey(const std::string& unique_id);

// Database status.
enum class DatabaseStatus { kOk, kFailed, kNotFound };

DatabaseStatus ToDatabaseStatus(blink::ServiceWorkerStatusCode status);

// Converts the |metadata_proto| to a BackgroundFetchRegistration object.
bool ToBackgroundFetchRegistration(
    const proto::BackgroundFetchMetadata& metadata_proto,
    blink::mojom::BackgroundFetchRegistrationData* registration_data);

bool MojoFailureReasonFromRegistrationProto(
    proto::BackgroundFetchRegistration_BackgroundFetchFailureReason
        proto_failure_reason,
    blink::mojom::BackgroundFetchFailureReason* failure_reason);

// Utility functions to make sure the request URLs are unique, since
// Cache Storage does not support duplicate URLs yet.
// Use `MakeCacheUrlUnique` before writing to the cache, and
// `RemoveUniqueParamFromCacheURL` when querying from the cache.
CONTENT_EXPORT GURL MakeCacheUrlUnique(const GURL& url,
                                       const std::string& unique_id,
                                       size_t request_index);
CONTENT_EXPORT GURL RemoveUniqueParamFromCacheURL(const GURL& url,
                                                  const std::string& unique_id);

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_HELPERS_H_
