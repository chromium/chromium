// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_helpers.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {
namespace background_fetch {

std::string ActiveRegistrationUniqueIdKey(const std::string& developer_id) {
  // Allows looking up the active registration's |unique_id| by |developer_id|.
  // Registrations are active from creation up until completed/failed/aborted.
  // These database entries correspond to the active background fetches map:
  // https://wicg.github.io/background-fetch/#service-worker-registration-active-background-fetches
  return kActiveRegistrationUniqueIdKeyPrefix + developer_id;
}

std::string RegistrationKey(const std::string& unique_id) {
  // Allows looking up a registration by |unique_id|.
  return kRegistrationKeyPrefix + unique_id;
}

std::string UIOptionsKey(const std::string& unique_id) {
  return kUIOptionsKeyPrefix + unique_id;
}

std::string PendingRequestKeyPrefix(const std::string& unique_id) {
  return kPendingRequestKeyPrefix + unique_id + kSeparator;
}

std::string PendingRequestKey(const std::string& unique_id, int request_index) {
  return PendingRequestKeyPrefix(unique_id) +
         base::NumberToString(request_index);
}

std::string ActiveRequestKeyPrefix(const std::string& unique_id) {
  return kActiveRequestKeyPrefix + unique_id + kSeparator;
}

std::string ActiveRequestKey(const std::string& unique_id, int request_index) {
  return ActiveRequestKeyPrefix(unique_id) +
         base::NumberToString(request_index);
}

std::string CompletedRequestKeyPrefix(const std::string& unique_id) {
  return kCompletedRequestKeyPrefix + unique_id + kSeparator;
}

std::string CompletedRequestKey(const std::string& unique_id,
                                int request_index) {
  return CompletedRequestKeyPrefix(unique_id) +
         base::NumberToString(request_index);
}

std::string StorageVersionKey(const std::string& unique_id) {
  return kStorageVersionKeyPrefix + unique_id;
}

DatabaseStatus ToDatabaseStatus(blink::ServiceWorkerStatusCode status) {
  switch (status) {
    case blink::ServiceWorkerStatusCode::kOk:
      return DatabaseStatus::kOk;
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      // kErrorFailed is for invalid arguments (e.g. empty key) or database
      // errors. kErrorAbort is for unexpected failures, e.g. because shutdown
      // is in progress. kErrorStorageDisconnected is for the Storage Service
      // disconnection. BackgroundFetchDataManager handles these the same way.
      return DatabaseStatus::kFailed;
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      // This can also happen for writes, if the ServiceWorkerRegistration has
      // been deleted.
      return DatabaseStatus::kNotFound;
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return DatabaseStatus::kFailed;
}

bool ToBackgroundFetchRegistration(
    const proto::BackgroundFetchMetadata& metadata_proto,
    blink::mojom::BackgroundFetchRegistrationData* registration_data) {
  DCHECK(registration_data);
  const auto& registration_proto = metadata_proto.registration();

  registration_data->developer_id = registration_proto.developer_id();
  registration_data->upload_total = registration_proto.upload_total();
  registration_data->uploaded = registration_proto.uploaded();
  registration_data->download_total = registration_proto.download_total();
  registration_data->downloaded = registration_proto.downloaded();
  switch (registration_proto.result()) {
    case proto::BackgroundFetchRegistration_BackgroundFetchResult_UNSET:
      registration_data->result = blink::mojom::BackgroundFetchResult::UNSET;
      break;
    case proto::BackgroundFetchRegistration_BackgroundFetchResult_FAILURE:
      registration_data->result = blink::mojom::BackgroundFetchResult::FAILURE;
      break;
    case proto::BackgroundFetchRegistration_BackgroundFetchResult_SUCCESS:
      registration_data->result = blink::mojom::BackgroundFetchResult::SUCCESS;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  bool did_convert = MojoFailureReasonFromRegistrationProto(
      registration_proto.failure_reason(), &registration_data->failure_reason);
  return did_convert;
}

blink::StorageKey GetMetadataStorageKey(
    const proto::BackgroundFetchMetadata& metadata_proto) {
  if (metadata_proto.has_storage_key()) {
    auto storage_key =
        blink::StorageKey::Deserialize(metadata_proto.storage_key());
    if (storage_key.has_value()) {
      return *storage_key;
    }
  }

  // Fall back to the deprecated `origin` field.
  if (metadata_proto.has_origin()) {
    return blink::StorageKey::CreateFirstParty(
        url::Origin::Create(GURL(metadata_proto.origin())));
  }

  // If neither field is set, the best we can do is an opaque StorageKey.
  return blink::StorageKey();
}

bool MojoFailureReasonFromRegistrationProto(
    proto::BackgroundFetchRegistration::BackgroundFetchFailureReason
        proto_failure_reason,
    blink::mojom::BackgroundFetchFailureReason* failure_reason) {
  DCHECK(failure_reason);
  switch (proto_failure_reason) {
    case proto::BackgroundFetchRegistration::NONE:
      *failure_reason = blink::mojom::BackgroundFetchFailureReason::NONE;
      return true;
    case proto::BackgroundFetchRegistration::CANCELLED_FROM_UI:
      *failure_reason =
          blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI;
      return true;
    case proto::BackgroundFetchRegistration::CANCELLED_BY_DEVELOPER:
      *failure_reason =
          blink::mojom::BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER;
      return true;
    case proto::BackgroundFetchRegistration::SERVICE_WORKER_UNAVAILABLE:
      *failure_reason = blink::mojom::BackgroundFetchFailureReason::
          SERVICE_WORKER_UNAVAILABLE;
      return true;
    case proto::BackgroundFetchRegistration::QUOTA_EXCEEDED:
      *failure_reason =
          blink::mojom::BackgroundFetchFailureReason::QUOTA_EXCEEDED;
      return true;
    case proto::BackgroundFetchRegistration::DOWNLOAD_TOTAL_EXCEEDED:
      *failure_reason =
          blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED;
      return true;
    case proto::BackgroundFetchRegistration::FETCH_ERROR:
      *failure_reason = blink::mojom::BackgroundFetchFailureReason::FETCH_ERROR;
      return true;
    case proto::BackgroundFetchRegistration::BAD_STATUS:
      *failure_reason = blink::mojom::BackgroundFetchFailureReason::BAD_STATUS;
      return true;
  }
  LOG(ERROR) << "BackgroundFetchFailureReason from the metadata proto doesn't"
             << " match any enum value. Possible database corruption.";
  return false;
}

GURL MakeCacheUrlUnique(const GURL& url,
                        const std::string& unique_id,
                        size_t request_index) {
  std::string query = url.query();
  query += unique_id + base::NumberToString(request_index);

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);

  return url.ReplaceComponents(replacements);
}

GURL RemoveUniqueParamFromCacheURL(const GURL& url,
                                   const std::string& unique_id) {
  std::vector<std::string> split = base::SplitStringUsingSubstr(
      url.query(), unique_id, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  GURL::Replacements replacements;
  if (split.size() == 1u)
    replacements.ClearQuery();
  else if (split.size() == 2u)
    replacements.SetQueryStr(split[0]);
  else
    NOTREACHED_IN_MIGRATION();

  return url.ReplaceComponents(replacements);
}

}  // namespace background_fetch
}  // namespace content
