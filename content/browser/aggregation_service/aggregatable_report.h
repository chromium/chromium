// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AggregatableReportRequest;

// The underlying private information which will be sent to the processing
// servers for aggregation. Each payload encodes a single contribution to a
// histogram bucket. This will be encrypted and won't be readable by the
// reporting endpoint.
struct CONTENT_EXPORT AggregationServicePayloadContents {
  // TODO(alexmt): Add kDistinctCount option.
  enum class Operation {
    kHistogram,
  };

  static constexpr size_t kMaximumFilteringIdMaxBytes = 8;

  // The default aggregation coordinator origin will be used if
  // `aggregation_coordinator_origin` is `std::nullopt`.
  // `max_contributions_allowed` specifies the maximum number of contributions
  // per report for use in padding. `filtering_id_max_bytes` specifies how many
  // bytes should be used for the filtering ID encoding.
  AggregationServicePayloadContents(
      Operation operation,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions,
      std::optional<url::Origin> aggregation_coordinator_origin,
      base::StrictNumeric<size_t> max_contributions_allowed,
      size_t filtering_id_max_bytes);

  AggregationServicePayloadContents(
      const AggregationServicePayloadContents& other);
  AggregationServicePayloadContents& operator=(
      const AggregationServicePayloadContents& other);
  AggregationServicePayloadContents(AggregationServicePayloadContents&& other);
  AggregationServicePayloadContents& operator=(
      AggregationServicePayloadContents&& other);
  ~AggregationServicePayloadContents();

  Operation operation;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  std::optional<url::Origin> aggregation_coordinator_origin;
  size_t max_contributions_allowed;
  size_t filtering_id_max_bytes;
};

// Represents the information that will be provided to both the reporting
// endpoint and the processing server(s), i.e. stored in the encrypted payload
// and in the plaintext report.
struct CONTENT_EXPORT AggregatableReportSharedInfo {
  enum class DebugMode {
    kDisabled,
    kEnabled,
  };

  AggregatableReportSharedInfo(base::Time scheduled_report_time,
                               base::Uuid report_id,
                               url::Origin reporting_origin,
                               DebugMode debug_mode,
                               base::Value::Dict additional_fields,
                               std::string api_version,
                               std::string api_identifier);

  AggregatableReportSharedInfo(const AggregatableReportSharedInfo& other) =
      delete;
  AggregatableReportSharedInfo& operator=(
      const AggregatableReportSharedInfo& other) = delete;
  AggregatableReportSharedInfo(AggregatableReportSharedInfo&& other);
  AggregatableReportSharedInfo& operator=(AggregatableReportSharedInfo&& other);
  ~AggregatableReportSharedInfo();

  // Creates a deep copy of this object.
  AggregatableReportSharedInfo Clone() const;

  // Serializes to a JSON dictionary, represented as a string.
  std::string SerializeAsJson() const;

  base::Time scheduled_report_time;
  base::Uuid report_id;  // Used to prevent double counting.
  url::Origin reporting_origin;
  DebugMode debug_mode;
  base::Value::Dict additional_fields;
  std::string api_version;

  // Enum string that indicates which API created the report.
  std::string api_identifier;
};

// An AggregatableReport contains all the information needed for sending the
// report to its reporting endpoint. All nested information has already been
// serialized and encrypted as necessary.
class CONTENT_EXPORT AggregatableReport {
 public:
  // This is used to encapsulate the data that is specific to a single
  // processing server.
  struct CONTENT_EXPORT AggregationServicePayload {
    AggregationServicePayload(
        std::vector<uint8_t> payload,
        std::string key_id,
        std::optional<std::vector<uint8_t>> debug_cleartext_payload);
    AggregationServicePayload(const AggregationServicePayload& other);
    AggregationServicePayload& operator=(
        const AggregationServicePayload& other);
    AggregationServicePayload(AggregationServicePayload&& other);
    AggregationServicePayload& operator=(AggregationServicePayload&& other);
    ~AggregationServicePayload();

    // This payload is constructed using the data in the
    // AggregationServicePayloadContents and then encrypted with one of
    // `url`'s public keys. The plaintext of the encrypted payload is a
    // serialized CBOR map structured as follows:
    // {
    //   "operation": "<chosen operation as string>",
    //   "data": [{
    //     "bucket": <a 16-byte (i.e. 128-bit) big-endian bytestring>,
    //     "value": <a 4-byte (i.e. 32-bit) big-endian bytestring>
    //   }, ...],
    // }
    std::vector<uint8_t> payload;

    // Indicates the chosen encryption key.
    std::string key_id;

    // If the request's shared info had a `kEnabled` debug_mode, contains the
    // cleartext payload for debugging. Otherwise, it is `std::nullopt`.
    std::optional<std::vector<uint8_t>> debug_cleartext_payload;
  };

  // Used to allow mocking `CreateFromRequestAndPublicKey()` in tests.
  class CONTENT_EXPORT Provider {
   public:
    virtual ~Provider();

    // Processes and serializes the information in `report_request` and encrypts
    // using the `public_key` as necessary. Returns `std::nullopt` if an error
    // occurred during construction.
    virtual std::optional<AggregatableReport> CreateFromRequestAndPublicKey(
        const AggregatableReportRequest& report_request,
        PublicKey public_key) const;

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
  static constexpr std::string_view kDomainSeparationPrefix =
      "aggregation_service";

  AggregatableReport(std::optional<AggregationServicePayload> payload,
                     std::string shared_info,
                     std::optional<uint64_t> debug_key,
                     base::flat_map<std::string, std::string> additional_fields,
                     std::optional<url::Origin> aggregation_coordinator_origin);
  AggregatableReport(const AggregatableReport& other);
  AggregatableReport& operator=(const AggregatableReport& other);
  AggregatableReport(AggregatableReport&& other);
  AggregatableReport& operator=(AggregatableReport&& other);
  ~AggregatableReport();

  const std::optional<AggregationServicePayload>& payload() const {
    return payload_;
  }
  std::string_view shared_info() const { return shared_info_; }
  std::optional<uint64_t> debug_key() const { return debug_key_; }
  const base::flat_map<std::string, std::string>& additional_fields() const {
    return additional_fields_;
  }
  const std::optional<url::Origin>& aggregation_coordinator_origin() const {
    return aggregation_coordinator_origin_;
  }

  // Returns the JSON representation of this report of the form
  // {
  //   "shared_info": "<shared_info>",
  //   "aggregation_service_payloads": [
  //     {
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     }
  //   ]
  // }
  //
  // Where <shared_info> is the serialization of the JSON (with all whitespace
  // removed):
  // {
  //   "report_id":"[UUID]",
  //   "reporting_origin":"https://reporter.example"
  //   "scheduled_report_time":"[timestamp in seconds]",
  //   "version":"[api version]",
  // }
  // Callers may insert API-specific fields into the shared_info dictionary.
  // In those cases, the keys are inserted in lexicographic order.
  //
  // If requested, each "aggregation_service_payloads" element has an extra
  // field: `"debug_cleartext_payload": "<base64 encoded payload cleartext>"`.
  // Note that APIs may wish to add additional key-value pairs to this returned
  // value. Additionally, if requested, the outer JSON will have an extra field:
  // `"debug_key": "<unsigned 64-bit integer>"` along with any other extra
  // fields specified in `additional_fields_`.
  base::Value::Dict GetAsJson() const;

  // TODO(crbug.com/40196851): Expose static method to validate that a
  // base::Value appears to represent a valid report.

  static std::optional<std::vector<uint8_t>> SerializePayloadForTesting(
      const AggregationServicePayloadContents& payload_contents);

  static std::optional<size_t> ComputePayloadLengthInBytesForTesting(
      size_t num_contributions,
      size_t filtering_id_max_bytes);

 private:
  // Might be `std::nullopt` for reports created for the WebUI if prior to
  // assembly or if assembly failed.
  std::optional<AggregationServicePayload> payload_;

  std::string shared_info_;

  // Should only be set if the debug mode is enabled (but can still be empty).
  // Used as part of the temporary debugging mechanism.
  std::optional<uint64_t> debug_key_;

  base::flat_map<std::string, std::string> additional_fields_;

  std::optional<url::Origin> aggregation_coordinator_origin_;
};

// Represents a request for an AggregatableReport. Contains all the data
// necessary to construct the report except for the PublicKey for each
// processing URL.
class CONTENT_EXPORT AggregatableReportRequest {
 public:
  // Rough categories of report scheduling delays used for metrics. Keep this
  // synchronized with `proto::AggregatableReportRequest::DelayType`. Do not
  // remove or renumber enumerators because protos containing these values are
  // persisted to disk.
  enum class DelayType : uint8_t {
    ScheduledWithReducedDelay = 0,
    ScheduledWithFullDelay = 1,
    Unscheduled = 2,

    kMinValue = ScheduledWithReducedDelay,
    kMaxValue = Unscheduled,
  };

  static constexpr std::string_view DelayTypeToString(DelayType delay_type) {
    switch (delay_type) {
      case DelayType::ScheduledWithReducedDelay:
        return "ScheduledWithReducedDelay";
      case DelayType::ScheduledWithFullDelay:
        return "ScheduledWithFullDelay";
      case DelayType::Unscheduled:
        return "Unscheduled";
    }
    NOTREACHED();
  }

  // Returns `std::nullopt` if any of the following are true:
  //
  //   * `payload_contents.max_contributions_allowed` is less than the number of
  //     contributions.
  //
  //   * Any contribution in `payload_contents` has a negative value.
  //
  //   * Any contribution's filtering ID does not fit in the given
  //     `payload_contents.filtering_id_max_bytes`.
  //
  //   * `payload_contents.filtering_id_max_bytes` contains a value that is
  //     either non-positive or greater than `kMaximumFilteringIdMaxBytes`.
  //
  //   * `shared_info.report_id` is invalid.
  //
  //   * `shared_info.debug_mode == kDisabled` and `debug_key` contains a value.
  //
  //   * `failed_send_attempts` is negative.
  //
  // TODO(alexmt): Add validation for `payload_contents.scheduled_report_time`
  // being non-null/inf.
  static std::optional<AggregatableReportRequest> Create(
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info,
      std::optional<AggregatableReportRequest::DelayType> delay_type =
          std::nullopt,
      std::string reporting_path = std::string(),
      std::optional<uint64_t> debug_key = std::nullopt,
      base::flat_map<std::string, std::string> additional_fields = {},
      int failed_send_attempts = 0);

  // Returns `std:nullopt` whenever `Create()` would for that condition too.
  static std::optional<AggregatableReportRequest> CreateForTesting(
      GURL processing_url,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info,
      std::optional<AggregatableReportRequest::DelayType> delay_type =
          std::nullopt,
      std::string reporting_path = std::string(),
      std::optional<uint64_t> debug_key = std::nullopt,
      base::flat_map<std::string, std::string> additional_fields = {},
      int failed_send_attempts = 0);

  // Deserializes a bytestring generated by `Serialize()`. Returns
  // `std::nullopt` in the case of a deserialization error.
  static std::optional<AggregatableReportRequest> Deserialize(
      base::span<const uint8_t> serialized_proto);

  // Move-only.
  AggregatableReportRequest(AggregatableReportRequest&& other);
  AggregatableReportRequest& operator=(AggregatableReportRequest&& other);
  ~AggregatableReportRequest();

  const GURL& processing_url() const { return processing_url_; }
  const AggregationServicePayloadContents& payload_contents() const {
    return payload_contents_;
  }
  const AggregatableReportSharedInfo& shared_info() const {
    return shared_info_;
  }
  std::string_view reporting_path() const { return reporting_path_; }
  std::optional<uint64_t> debug_key() const { return debug_key_; }
  const base::flat_map<std::string, std::string>& additional_fields() const {
    return additional_fields_;
  }
  int failed_send_attempts() const { return failed_send_attempts_; }
  std::optional<DelayType> delay_type() const { return delay_type_; }

  // Returns the URL this report should be sent to. The return value is invalid
  // if the reporting_path is empty.
  GURL GetReportingUrl() const;

  // Serializes the report request to a binary protobuf encoding. Crashes when
  // `delay_type()` is empty or equals `DelayType::Unscheduled`. Returns an
  // empty vector when proto serialization fails.
  std::vector<uint8_t> Serialize() const;

 private:
  static std::optional<AggregatableReportRequest> CreateInternal(
      GURL processing_url,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      std::string reporting_path,
      std::optional<uint64_t> debug_key,
      base::flat_map<std::string, std::string> additional_fields,
      int failed_send_attempts);

  AggregatableReportRequest(
      GURL processing_url,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info,
      std::optional<AggregatableReportRequest::DelayType> delay_type,
      std::string reporting_path,
      std::optional<uint64_t> debug_key,
      base::flat_map<std::string, std::string> additional_fields,
      int failed_send_attempts);

  GURL processing_url_;
  AggregationServicePayloadContents payload_contents_;
  AggregatableReportSharedInfo shared_info_;

  // The URL path where the assembled report should be sent (when combined with
  // `shared_info_.reporting_origin`). If the `AggregatableReportSender` is not
  // being used, this can be left empty.
  std::string reporting_path_;

  // Can only be set if `shared_info_.debug_mode` is `kEnabled` (but can still
  // be empty). Used as part of the temporary debugging mechanism.
  std::optional<uint64_t> debug_key_;

  base::flat_map<std::string, std::string> additional_fields_;

  // Number of times the browser has tried and failed to send this report before
  // this attempt. The value in this class is not incremented if this attempt
  // fails (until a new object is requested from storage)
  int failed_send_attempts_ = 0;

  // The rough category of report scheduling delay selected when this report
  // request was first created. This field should be set to `std::nullopt` for
  // requests that do not pass through the scheduler or network sender.
  // `Deserialize()` will set this to `std::nullopt` when parsing a protobuf
  // that was serialized before the addition of this field.
  std::optional<AggregatableReportRequest::DelayType> delay_type_;
};

CONTENT_EXPORT GURL GetAggregationServiceProcessingUrl(const url::Origin&);

// Encrypts the `report_payload_plaintext` with HPKE using the processing url's
// `public_key`. Returns empty vector if the encryption fails.
CONTENT_EXPORT std::vector<uint8_t> EncryptAggregatableReportPayloadWithHpke(
    base::span<const uint8_t> report_payload_plaintext,
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> report_authenticated_info);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
