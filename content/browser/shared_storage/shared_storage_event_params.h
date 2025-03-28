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
#include "url/gurl.h"

namespace content {

// Bundles the varying possible parameters for DevTools shared storage access
// events.
class CONTENT_EXPORT SharedStorageEventParams {
 public:
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
      const blink::CloneableMessage& serialized_data,
      int worklet_id);
  static SharedStorageEventParams CreateForSelectURL(
      const std::string& operation_name,
      const blink::CloneableMessage& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
      int worklet_id);

  static SharedStorageEventParams CreateForSet(
      const std::string& key,
      const std::string& value,
      bool ignore_if_present,
      std::optional<int> worklet_id = std::nullopt);
  static SharedStorageEventParams CreateForAppend(
      const std::string& key,
      const std::string& value,
      std::optional<int> worklet_id = std::nullopt);
  static SharedStorageEventParams CreateForGetOrDelete(
      const std::string& key,
      std::optional<int> worklet_id = std::nullopt);

  static SharedStorageEventParams CreateWithWorkletId(int worklet_id);
  static SharedStorageEventParams CreateDefault();

  SharedStorageEventParams(const SharedStorageEventParams&);
  ~SharedStorageEventParams();
  SharedStorageEventParams& operator=(const SharedStorageEventParams&);

  std::optional<std::string> script_source_url;
  std::optional<std::string> data_origin;
  std::optional<std::string> operation_name;
  std::optional<std::string> serialized_data;
  std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
      urls_with_metadata;
  std::optional<std::string> key;
  std::optional<std::string> value;
  std::optional<bool> ignore_if_present;
  std::optional<int> worklet_id;

 private:
  SharedStorageEventParams();
  SharedStorageEventParams(
      std::optional<std::string> script_source_url,
      std::optional<std::string> data_origin,
      std::optional<std::string> operation_name,
      std::optional<std::string> serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      std::optional<int> worklet_id);

  static SharedStorageEventParams CreateForWorkletCreation(
      const GURL& script_source_url,
      std::optional<std::string> data_origin,
      int worklet_id);

  static SharedStorageEventParams CreateForWorkletOperation(
      const std::string& operation_name,
      const blink::CloneableMessage& serialized_data,
      std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      int worklet_id);

  static SharedStorageEventParams CreateForModifierMethod(
      std::optional<std::string> key,
      std::optional<std::string> value,
      std::optional<bool> ignore_if_present,
      std::optional<int> worklet_id);
};

CONTENT_EXPORT bool operator==(const SharedStorageEventParams& lhs,
                               const SharedStorageEventParams& rhs);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& os,
                                        const SharedStorageEventParams& params);

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
