// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_

#include <map>
#include <optional>
#include <ostream>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Bundles the varying possible parameters for DevTools shared storage access
// events.
class CONTENT_EXPORT SharedStorageEventParams {
 public:
  // Wraps a `blink::mojom::PrivateAggregationConfig` for DevTools shared
  // storage integration.
  struct CONTENT_EXPORT PrivateAggregationConfigWrapper {
    blink::mojom::PrivateAggregationConfigPtr config;
    PrivateAggregationConfigWrapper();
    PrivateAggregationConfigWrapper(
        const std::optional<url::Origin>& aggregation_coordinator_origin,
        const std::optional<std::string>& context_id,
        uint32_t filtering_id_max_bytes,
        std::optional<uint16_t> max_contributions);
    explicit PrivateAggregationConfigWrapper(
        const blink::mojom::PrivateAggregationConfigPtr& config);
    PrivateAggregationConfigWrapper(
        const PrivateAggregationConfigWrapper& other);
    ~PrivateAggregationConfigWrapper();
    PrivateAggregationConfigWrapper& operator=(
        const PrivateAggregationConfigWrapper& other);
    bool operator==(const PrivateAggregationConfigWrapper&) const;
    friend std::ostream& operator<<(
        std::ostream& os,
        const PrivateAggregationConfigWrapper& config);
  };
  // Bundles a URL's spec along with a map of any accompanying reporting
  // metadata for DevTools integration.
  struct CONTENT_EXPORT SharedStorageUrlSpecWithMetadata {
    std::string url;
    std::map<std::string, std::string> reporting_metadata;
    SharedStorageUrlSpecWithMetadata();
    SharedStorageUrlSpecWithMetadata(
        const GURL& url,
        std::map<std::string, std::string> reporting_metadata);
    SharedStorageUrlSpecWithMetadata(const SharedStorageUrlSpecWithMetadata&);
    ~SharedStorageUrlSpecWithMetadata();
    SharedStorageUrlSpecWithMetadata& operator=(
        const SharedStorageUrlSpecWithMetadata&);
    bool operator==(const SharedStorageUrlSpecWithMetadata&) const;
    friend std::ostream& operator<<(
        std::ostream& os,
        const SharedStorageUrlSpecWithMetadata& url_with_metadata);
  };

  static SharedStorageEventParams CreateForAddModule(
      const GURL& script_source_url,
      int worklet_ordinal,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForCreateWorklet(
      const GURL& script_source_url,
      const std::string& data_origin,
      int worklet_ordinal,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForRun(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForRunForTesting(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForSelectURL(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
      bool resolve_to_config,
      std::string saved_query,
      const GURL& urn_uuid,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForSelectURLForTesting(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
      bool resolve_to_config,
      std::string saved_query,
      const GURL& urn_uuid,
      const base::UnguessableToken& worklet_devtools_token);

  static SharedStorageEventParams CreateForSet(
      const std::string& key,
      const std::string& value,
      bool ignore_if_present,
      const base::UnguessableToken& worklet_devtools_token =
          base::UnguessableToken::Null(),
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForAppend(
      const std::string& key,
      const std::string& value,
      const base::UnguessableToken& worklet_devtools_token =
          base::UnguessableToken::Null(),
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForDelete(
      const std::string& key,
      const base::UnguessableToken& worklet_devtools_token =
          base::UnguessableToken::Null(),
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForClear(
      const base::UnguessableToken& worklet_devtools_token =
          base::UnguessableToken::Null(),
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);

  static SharedStorageEventParams CreateForGet(
      const std::string& key,
      const base::UnguessableToken& worklet_devtools_token =
          base::UnguessableToken::Null());
  static SharedStorageEventParams CreateWithWorkletToken(
      const base::UnguessableToken& worklet_devtools_token);

  static SharedStorageEventParams CreateForBatchUpdate(
      const base::UnguessableToken& worklet_devtools_token,
      std::optional<std::string> with_lock,
      int batch_update_id,
      size_t batch_size);

  SharedStorageEventParams(const SharedStorageEventParams&);
  ~SharedStorageEventParams();
  SharedStorageEventParams& operator=(const SharedStorageEventParams&);

  std::optional<std::string> script_source_url;
  std::optional<std::string> data_origin;
  std::optional<std::string> operation_name;
  std::optional<int> operation_id;
  std::optional<bool> keep_alive;
  std::optional<PrivateAggregationConfigWrapper> private_aggregation_config;
  std::optional<std::string> serialized_data;
  std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
      urls_with_metadata;
  std::optional<bool> resolve_to_config;
  std::optional<std::string> saved_query;
  std::optional<std::string> urn_uuid;
  std::optional<std::string> key;
  std::optional<std::string> value;
  std::optional<bool> ignore_if_present;
  std::optional<int> worklet_ordinal;
  base::UnguessableToken worklet_devtools_token;
  std::optional<std::string> with_lock;
  std::optional<int> batch_update_id;
  std::optional<int> batch_size;

 private:
  SharedStorageEventParams();
  SharedStorageEventParams(
      std::optional<std::string> script_source_url,
      std::optional<std::string> data_origin,
      std::optional<std::string> operation_name,
      std::optional<int> operation_id,
      std::optional<bool> keep_alive,
      std::optional<PrivateAggregationConfigWrapper> private_aggregation_config,
      std::optional<std::string> serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      std::optional<std::string> urn_uuid,
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      std::optional<int> worklet_ordinal,
      const base::UnguessableToken& worklet_devtools_token,
      std::optional<std::string> with_lock,
      std::optional<int> batch_update_id,
      std::optional<int> batch_size);

  static SharedStorageEventParams CreateForWorkletCreation(
      const GURL& script_source_url,
      std::optional<std::string> data_origin,
      int worklet_ordinal,
      const base::UnguessableToken& worklet_devtools_token);

  static SharedStorageEventParams CreateForWorkletOperation(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      std::optional<std::string> urn_uuid,
      const base::UnguessableToken& worklet_devtools_token);
  static SharedStorageEventParams CreateForWorkletOperationForTesting(
      const std::string& operation_name,
      int operation_id,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      std::optional<std::string> urn_uuid,
      const base::UnguessableToken& worklet_devtools_token);

  static SharedStorageEventParams CreateForModifierMethod(
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      const base::UnguessableToken& worklet_devtools_token,
      std::optional<std::string> with_lock,
      std::optional<int> batch_update_id);

  static SharedStorageEventParams CreateForGetterMethod(
      std::optional<std::string> key,
      const base::UnguessableToken& worklet_devtools_token);
};

CONTENT_EXPORT bool operator==(const SharedStorageEventParams& lhs,
                               const SharedStorageEventParams& rhs);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& os,
                                        const SharedStorageEventParams& params);

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
