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

#include "base/guid.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
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
                                    absl::uint128 bucket,
                                    int value,
                                    ProcessingType processing_type);

  Operation operation;
  absl::uint128 bucket;
  int value;
  ProcessingType processing_type;
};

// Represents the information that will be provided to both the reporting
// endpoint and the processing origin(s), i.e. stored in the encrypted payload
// and in the plaintext report.
struct CONTENT_EXPORT AggregatableReportSharedInfo {
  enum class DebugMode {
    kDisabled,
    kEnabled,
  };

  AggregatableReportSharedInfo(base::Time scheduled_report_time,
                               std::string privacy_budget_key,
                               base::GUID report_id,
                               url::Origin reporting_origin,
                               DebugMode debug_mode);
  AggregatableReportSharedInfo(const AggregatableReportSharedInfo& other);
  AggregatableReportSharedInfo& operator=(
      const AggregatableReportSharedInfo& other);
  AggregatableReportSharedInfo(AggregatableReportSharedInfo&& other);
  AggregatableReportSharedInfo& operator=(AggregatableReportSharedInfo&& other);
  ~AggregatableReportSharedInfo();

  // Serializes to a JSON dictionary, represented as a string.
  std::string SerializeAsJson() const;

  base::Time scheduled_report_time;
  std::string privacy_budget_key;
  base::GUID report_id;  // Used to prevent double counting.
  url::Origin reporting_origin;
  DebugMode debug_mode;
};

// An AggregatableReport contains all the information needed for sending the
// report to its reporting endpoint. All nested information has already been
// serialized and encrypted as necessary.
class CONTENT_EXPORT AggregatableReport {
 public:
  // This is used to encapsulate the data that is specific to a single
  // processing origin.
  struct CONTENT_EXPORT AggregationServicePayload {
    AggregationServicePayload(
        std::vector<uint8_t> payload,
        std::string key_id,
        absl::optional<std::vector<uint8_t>> debug_cleartext_payload);
    AggregationServicePayload(const AggregationServicePayload& other);
    AggregationServicePayload& operator=(
        const AggregationServicePayload& other);
    AggregationServicePayload(AggregationServicePayload&& other);
    AggregationServicePayload& operator=(AggregationServicePayload&& other);
    ~AggregationServicePayload();

    // This payload is constructed using the data in the
    // AggregationServicePayloadContents and then encrypted with one of
    // `origin`'s public keys. For the `kSingleServer` processing type, the
    // plaintext of the encrypted payload is a serialized CBOR map structured as
    // follows:
    // {
    //   "operation": "<chosen operation as string>",
    //   "data": [{
    //     "bucket": <128-bit integer encoded as a big-endian bytestring>,
    //     "value": <integer>,
    //   }],
    // }
    // For the `kTwoParty` processing type, the "data" field is replaced with:
    //   "dpf_key": <binary serialization of the DPF key>
    std::vector<uint8_t> payload;

    // Indicates the chosen encryption key.
    std::string key_id;

    // If the request's shared info had a `kEnabled` debug_mode, contains the
    // cleartext payload for debugging. Otherwise, it is `absl::nullopt`.
    absl::optional<std::vector<uint8_t>> debug_cleartext_payload;
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

  // Used as a prefix for the authenticated information (i.e. context info).
  // This value must not be reused for new protocols or versions of this
  // protocol unless the ciphertexts are intended to be compatible. This ensures
  // that, even if public keys are reused, the same ciphertext cannot be (i.e.
  // no cross-protocol attacks).
  static constexpr char kDomainSeparationPrefix[] = "aggregation_service";

  AggregatableReport(std::vector<AggregationServicePayload> payloads,
                     std::string shared_info);
  AggregatableReport(const AggregatableReport& other);
  AggregatableReport& operator=(const AggregatableReport& other);
  AggregatableReport(AggregatableReport&& other);
  AggregatableReport& operator=(AggregatableReport&& other);
  ~AggregatableReport();

  const std::vector<AggregationServicePayload>& payloads() const {
    return payloads_;
  }
  const std::string& shared_info() const { return shared_info_; }

  // Returns the JSON representation of this report of the form
  // {
  //   "shared_info": "{\"scheduled_report_time\":\"[timestamp in
  //   seconds]\",\"privacy_budget_key\":\"[string]\",\"version\":\"[api
  //   version]\",\"report_id\":\"[UUID]\",\"reporting_origin\":\"[string]\"}",
  //   "aggregation_service_payloads": [
  //     {
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     },
  //     {
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     }
  //   ]
  // }
  // If requested, each "aggregation_service_payloads" element has an extra
  // field: `"debug_cleartext_payload": "<base64 encoded payload cleartext>"`.
  // Note that APIs may wish to add additional key-value pairs to this returned
  // value.
  base::Value::DictStorage GetAsJson() const;

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

  std::string shared_info_;
};

// Represents a request for an AggregatableReport. Contains all the data
// necessary to construct the report except for the PublicKey for each
// processing origin.
class CONTENT_EXPORT AggregatableReportRequest {
 public:
  // Returns `absl::nullopt` if `payload_contents` has a negative value. Also
  // returns `absl::nullopt` if `shared_info.report_id` is not valid.
  static absl::optional<AggregatableReportRequest> Create(
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  // Returns `absl::nullopt` if `payload_contents` has a negative value. Also
  // returns `absl::nullopt` if `processing_origins.size()` is not valid for the
  // `payload_contents.processing_type` (see `IsNumberOfProcessingOriginsValid`
  // above). Also returns `absl::nullopt` if `shared_info.report_id` is not
  // valid.
  static absl::optional<AggregatableReportRequest> CreateForTesting(
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

  static absl::optional<AggregatableReportRequest> CreateInternal(
      std::vector<url::Origin> processing_origins,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  AggregatableReportRequest(std::vector<url::Origin> processing_origins,
                            AggregationServicePayloadContents payload_contents,
                            AggregatableReportSharedInfo shared_info);

  std::vector<url::Origin> processing_origins_;
  AggregationServicePayloadContents payload_contents_;
  AggregatableReportSharedInfo shared_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
