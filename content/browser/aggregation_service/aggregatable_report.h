// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class AggregatableReportRequest;

// The underlying private information which will be sent to the processing
// origins for aggregation. Each payload encodes a single contribution to a
// histogram bucket. This will be encrypted and won't be readable by the
// reporting endpoint.
struct CONTENT_EXPORT AggregationServicePayloadContents {
  // TODO(alexmt): Add kDistinctCount option.
  enum class Operation {
    kHistogram = 0,
    kMaxValue = kHistogram,
  };

  enum class ProcessingType {
    kTwoParty = 0,
    kSingleServer = 1,
    kMaxValue = kSingleServer,
  };

  AggregationServicePayloadContents(Operation operation,
                                    int bucket,
                                    int value,
                                    ProcessingType processing_type,
                                    url::Origin reporting_origin);

  Operation operation;
  int bucket;
  int value;
  ProcessingType processing_type;
  url::Origin reporting_origin;
};

// Represents the information that will be provided to both the reporting
// endpoint and the processing origin(s), i.e. stored in the encrypted payload
// and in the plaintext report.
struct CONTENT_EXPORT AggregatableReportSharedInfo {
  AggregatableReportSharedInfo(base::Time scheduled_report_time,
                               std::string privacy_budget_key);
  base::Time scheduled_report_time;
  std::string privacy_budget_key;
};

// An AggregatableReport contains all the information needed for sending the
// report to its reporting endpoint. All nested information has already been
// serialized and encrypted as necessary.
class CONTENT_EXPORT AggregatableReport {
 public:
  // This is used to encapsulate the data that is specific to a single
  // processing origin.
  struct CONTENT_EXPORT AggregationServicePayload {
    AggregationServicePayload(url::Origin origin,
                              std::vector<uint8_t> payload,
                              std::string key_id);
    AggregationServicePayload(AggregationServicePayload&& other);
    AggregationServicePayload& operator=(AggregationServicePayload&& other);
    ~AggregationServicePayload();

    url::Origin origin;

    // This payload is constructed using the data in the
    // AggregationServicePayloadContents and then encrypted with one of
    // `origin`'s public keys. For the kTwoParty processing type, the plaintext
    // of the encrypted payload is a serialized CBOR map structured as follows:
    // {
    //   "version": "<API version>",
    //   "operation": "<chosen operation as string>",
    //   "privacy_budget_key": "<field for server to do privacy budgeting>",
    //   "scheduled_report_time": <timestamp in msec>,
    //   "reporting_origin": "https://reporter.example",
    //   "dpf_key": <binary serialization of the DPF key>,
    // }
    // For the kSingleServer processing type, the "dpf_key" field is replaced
    // with:
    //   "data": [{ "bucket": <bucket>, "value": <value> }]
    // If two processing origins are provided, one payload (chosen randomly)
    // would contain that data and the other would instead contain:
    //   "data": []
    std::vector<uint8_t> payload;

    // Indicates the chosen encryption key.
    std::string key_id;
  };

  // Used to allow mocking `CreateFromRequestAndPublicKeys()` in tests.
  class CONTENT_EXPORT Provider {
   public:
    virtual ~Provider();

    // Processes and serializes the information in `report_request` and encrypts
    // using the `public_keys` as necessary. The order of `public_keys` should
    // correspond to `report_request.processing_origins`, which should be
    // sorted. Returns `absl::nullopt` if an error occurred during construction.
    virtual absl::optional<AggregatableReport> CreateFromRequestAndPublicKeys(
        AggregatableReportRequest report_request,
        std::vector<PublicKey> public_keys) const;

    // Sets whether to disable encryption of the payload(s). Should only be used
    // by the AggregationServiceTool.
    static void SetDisableEncryptionForTestingTool(bool should_disable);

   private:
    static bool g_disable_encryption_for_testing_tool_;
  };

  // log_2 of the number of buckets
  static constexpr size_t kBucketDomainBitLength = 32;

  // log_2 of the value output space
  static constexpr size_t kValueDomainBitLength = 64;

  // Used as the authenticated information (i.e. context info). This value must
  // not be reused for new protocols or versions of this protocol unless the
  // ciphertexts are intended to be compatible. This ensures that, even if
  // public keys are reused, the same ciphertext cannot be (i.e. no cross-
  // protocol attacks).
  static constexpr char kDomainSeparationValue[] = "aggregation_service";

  AggregatableReport(std::vector<AggregationServicePayload> payloads,
                     AggregatableReportSharedInfo shared_info);
  // Move-only.
  AggregatableReport(AggregatableReport&& other);
  AggregatableReport& operator=(AggregatableReport&& other);
  ~AggregatableReport();

  const std::vector<AggregationServicePayload>& payloads() const {
    return payloads_;
  }
  const AggregatableReportSharedInfo& shared_info() const {
    return shared_info_;
  }

  // Returns the JSON representation of this report of the form
  // {
  //   "scheduled_report_time": "<timestamp in msec>",
  //   "privacy_budget_key": "<field for server to do privacy budgeting>",
  //   "version": "<api version>",
  //   "aggregation_service_payloads": [
  //     {
  //       "origin": "https://helper1.example",
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     },
  //     {
  //       "origin": "https://helper2.example",
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     }
  //   ]
  // }
  // Note that APIs may wish to add additional key-value pairs to this returned
  // value. `this` is required to be an rvalue to avoid unnecessary copies; this
  // method should only need to be called once.
  base::Value::DictStorage GetAsJson() &&;

  // TODO(crbug.com/1247409): Expose static method to validate that a
  // base::Value appears to represent a valid report.

  // Returns whether `number` is a valid number of processing origins for the
  // `processing_type`.
  static bool IsNumberOfProcessingOriginsValid(
      size_t number,
      AggregationServicePayloadContents::ProcessingType processing_type);

 private:
  // This vector should have an entry for each processing origin specified in
  // the original AggregatableReportRequest.
  std::vector<AggregationServicePayload> payloads_;

  AggregatableReportSharedInfo shared_info_;
};

// Represents a request for an AggregatableReport. Contains all the data
// necessary to construct the report except for the PublicKey for each
// processing origin.
class CONTENT_EXPORT AggregatableReportRequest {
 public:
  // Returns `absl::nullopt` if `payload_contents` has a negative bucket or
  // value. Also returns `absl::nullopt` if `processing_origins.size()` is not
  // valid for the `payload_contents.processing_type` (see
  // `IsNumberOfProcessingOriginsValid` above).
  static absl::optional<AggregatableReportRequest> Create(
      std::vector<url::Origin> processing_origins,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  // Move-only.
  AggregatableReportRequest(AggregatableReportRequest&& other);
  AggregatableReportRequest& operator=(AggregatableReportRequest&& other);
  ~AggregatableReportRequest();

  const std::vector<url::Origin>& processing_origins() const {
    return processing_origins_;
  }
  const AggregationServicePayloadContents& payload_contents() const {
    return payload_contents_;
  }
  const AggregatableReportSharedInfo& shared_info() const {
    return shared_info_;
  }

 private:
  // To avoid unnecessary copies, allow the provider to directly access members
  // of the AggregatableReportRequest being consumed.
  friend class AggregatableReport::Provider;

  AggregatableReportRequest(std::vector<url::Origin> processing_origins,
                            AggregationServicePayloadContents payload_contents,
                            AggregatableReportSharedInfo shared_info);

  std::vector<url::Origin> processing_origins_;
  AggregationServicePayloadContents payload_contents_;
  AggregatableReportSharedInfo shared_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
