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
      int worklet_id);
  static SharedStorageEventParams CreateForCreateWorklet(
      const GURL& script_source_url,
      const std::string& data_origin,
      int worklet_id);
  static SharedStorageEventParams CreateForRun(
      const std::string& operation_name,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      int worklet_id);
  static SharedStorageEventParams CreateForRunForTesting(
      const std::string& operation_name,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      int worklet_id);
  static SharedStorageEventParams CreateForSelectURL(
      const std::string& operation_name,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
      bool resolve_to_config,
      std::string saved_query,
      int worklet_id);
  static SharedStorageEventParams CreateForSelectURLForTesting(
      const std::string& operation_name,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
      bool resolve_to_config,
      std::string saved_query,
      int worklet_id);

  static SharedStorageEventParams CreateForSet(
      const std::string& key,
      const std::string& value,
      bool ignore_if_present,
      std::optional<int> worklet_id = std::nullopt,
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForAppend(
      const std::string& key,
      const std::string& value,
      std::optional<int> worklet_id = std::nullopt,
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForDelete(
      const std::string& key,
      std::optional<int> worklet_id = std::nullopt,
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);
  static SharedStorageEventParams CreateForClear(
      std::optional<int> worklet_id = std::nullopt,
      std::optional<std::string> with_lock = std::nullopt,
      std::optional<int> batch_update_id = std::nullopt);

  static SharedStorageEventParams CreateForGet(
      const std::string& key,
      std::optional<int> worklet_id = std::nullopt);
  static SharedStorageEventParams CreateWithWorkletId(int worklet_id);

  SharedStorageEventParams(const SharedStorageEventParams&);
  ~SharedStorageEventParams();
  SharedStorageEventParams& operator=(const SharedStorageEventParams&);

  std::optional<std::string> script_source_url;
  std::optional<std::string> data_origin;
  std::optional<std::string> operation_name;
  std::optional<bool> keep_alive;
  std::optional<PrivateAggregationConfigWrapper> private_aggregation_config;
  std::optional<std::string> serialized_data;
  std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
      urls_with_metadata;
  std::optional<bool> resolve_to_config;
  std::optional<std::string> saved_query;
  std::optional<std::string> key;
  std::optional<std::string> value;
  std::optional<bool> ignore_if_present;
  std::optional<int> worklet_id;
  std::optional<std::string> with_lock;
  std::optional<int> batch_update_id;

 private:
  SharedStorageEventParams();
  SharedStorageEventParams(
      std::optional<std::string> script_source_url,
      std::optional<std::string> data_origin,
      std::optional<std::string> operation_name,
      std::optional<bool> keep_alive,
      std::optional<PrivateAggregationConfigWrapper> private_aggregation_config,
      std::optional<std::string> serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      std::optional<int> worklet_id,
      std::optional<std::string> with_lock,
      std::optional<int> batch_update_id);

  static SharedStorageEventParams CreateForWorkletCreation(
      const GURL& script_source_url,
      std::optional<std::string> data_origin,
      int worklet_id);

  static SharedStorageEventParams CreateForWorkletOperation(
      const std::string& operation_name,
      bool keep_alive,
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config,
      const blink::CloneableMessage& serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      int worklet_id);
  static SharedStorageEventParams CreateForWorkletOperationForTesting(
      const std::string& operation_name,
      bool keep_alive,
      PrivateAggregationConfigWrapper config_wrapper,
      const blink::CloneableMessage& serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<bool> resolve_to_config,
      std::optional<std::string> saved_query,
      int worklet_id);

  static SharedStorageEventParams CreateForModifierMethod(
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      std::optional<int> worklet_id,
      std::optional<std::string> with_lock,
      std::optional<int> batch_update_id);

  static SharedStorageEventParams CreateForGetterMethod(
      std::optional<std::string> key,
      std::optional<int> worklet_id);
};

CONTENT_EXPORT bool operator==(const SharedStorageEventParams& lhs,
                               const SharedStorageEventParams& rhs);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& os,
                                        const SharedStorageEventParams& params);

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
