// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_
#define CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/origin.h"

class GURL;

template <class T>
class scoped_refptr;

namespace base {
class Clock;
class FilePath;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

// Interface for a test aggregation service which can be created without any
// dependencies. Supports configuring public keys at runtime.
class TestAggregationService {
 public:
  // TODO(crbug.com/40201850): Consider exposing AggregatableReportRequest in
  // content/public to avoid this translation.

  // This is 1-1 mapping of AggregationServicePayloadContents::Operation.
  enum class Operation {
    kHistogram,
  };

  // This is 1-1 mapping of AggregationServicePayloadContent::AggregationMode.
  enum class AggregationMode {
    kTeeBased,
    kExperimentalPoplar,
    kDefault = kTeeBased,
  };

  // Represents a request to assemble an aggregatable report.
  struct AssembleRequest {
    AssembleRequest(Operation operation,
                    absl::uint128 bucket,
                    int value,
                    AggregationMode aggregation_mode,
                    url::Origin reporting_origin,
                    std::vector<GURL> processing_urls,
                    bool is_debug_mode_enabled,
                    base::Value::Dict additional_fields,
                    std::string api_version,
                    std::string api_identifier);
    AssembleRequest(AssembleRequest&& other);
    AssembleRequest& operator=(AssembleRequest&& other);
    ~AssembleRequest();

    // Specifies the operation for the aggregation.
    Operation operation;
    // Specifies the bucket key of the histogram contribution.
    absl::uint128 bucket;
    // Specifies the bucket value of the histogram contribution.
    int value;
    // Specifies the aggregation mode to use.
    AggregationMode aggregation_mode;
    // Specifies the endpoint reporting origin.
    url::Origin reporting_origin;
    // Specifies the key for the aggregation servers to do privacy budgeting.
    std::string privacy_budget_key;
    // Specifies the aggregation server URLs.
    std::vector<GURL> processing_urls;
    // Whether debug_mode should be enabled for the report.
    bool is_debug_mode_enabled;

    // Additional fields to add to shared_info.
    base::Value::Dict additional_fields;

    // Specifies the API version.
    std::string api_version;

    // Enum string identifying which API created the report request.
    std::string api_identifier;
  };

  virtual ~TestAggregationService() = default;

  // Creates an instance of the service. Aggregatable reports will be sent
  // using the provided `url_loader_factory`.
  static std::unique_ptr<TestAggregationService> Create(
      const base::Clock* clock,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Sets whether to disable the AggregatableReport's payload(s) being encrypted
  // after serialization.
  virtual void SetDisablePayloadEncryption(bool should_disable) = 0;

  // Parses the keys for `url` from `json_file`, and saves the set of keys
  // to storage. `callback` will be run once completed which takes a boolean
  // value indicating whether the keys were parsed successfully.
  virtual void SetPublicKeys(const GURL& url,
                             const base::FilePath& json_file,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Construct an aggregatable report from the information in `request`.
  // `callback` will be run once completed which takes a `base::Value::Dict` for
  // the JSON representation of the aggregatable report. Empty
  // `base::Value::Dict` will be returned in case of error.
  virtual void AssembleReport(
      AssembleRequest request,
      base::OnceCallback<void(base::Value::Dict)> callback) = 0;

  // Sends the aggregatable report to the specified reporting endpoint `url`.
  // `callback` will be run once completed which returns whether the report was
  // sent successfully.
  virtual void SendReport(const GURL& url,
                          const base::Value& contents,
                          base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_
