// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_REPORT_UTIL_H_

#include <optional>
#include <string>

#include "base/uuid.h"
#include "components/cbor/values.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
namespace content {

// This class is meant to hold all the data necessary for building a request
// for the Private Model Training API.
class CONTENT_EXPORT PrivateModelTrainingRequest {
 public:
  static constexpr size_t kPublicKeyByteLength = X25519_PUBLIC_VALUE_LEN;
  // Used as a prefix for the authenticated information (i.e. context info).
  // This value must not be reused for new protocols or versions of this
  // protocol unless the ciphertexts are intended to be compatible. This ensures
  // that, even if public keys are reused, the same ciphertext cannot be (i.e.
  // no cross-protocol attacks).
  static constexpr std::string_view kDomainSeparationPrefix =
      "private_model_training";

  PrivateModelTrainingRequest(
      auction_worklet::mojom::PrivateModelTrainingRequestDataPtr
          pmt_request_data,
      url::Origin reporting_origin,
      BiddingAndAuctionServerKey public_key);

  PrivateModelTrainingRequest(PrivateModelTrainingRequest&&);
  PrivateModelTrainingRequest& operator=(PrivateModelTrainingRequest&&);

  PrivateModelTrainingRequest(const PrivateModelTrainingRequest&) = delete;
  PrivateModelTrainingRequest& operator=(const PrivateModelTrainingRequest&) =
      delete;

  ~PrivateModelTrainingRequest();

  static PrivateModelTrainingRequest CreateRequestForTesting(
      auction_worklet::mojom::PrivateModelTrainingRequestDataPtr
          pmt_request_data,
      url::Origin reporting_origin,
      BiddingAndAuctionServerKey public_key,
      base::Uuid report_id,
      base::Time scheduled_report_time);

  // Handles serializing the entire request into CBOR.
  std::optional<std::vector<uint8_t>> SerializeAndEncryptRequest();

  // Returns shared info as CBOR prefixed with the `kDomainSeparationPrefix`.
  std::optional<std::vector<uint8_t>> GetSharedInfoCborWithPrefix();

 private:
  PrivateModelTrainingRequest(
      auction_worklet::mojom::PrivateModelTrainingRequestDataPtr
          pmt_request_data,
      url::Origin reporting_origin,
      BiddingAndAuctionServerKey public_key,
      base::Uuid report_id,
      base::Time scheduled_report_time);

  struct SharedInfo {
    SharedInfo(base::Uuid report_id,
               url::Origin reporting_origin,
               base::Time scheduled_report_time);
    ~SharedInfo();
    SharedInfo(const SharedInfo&);
    SharedInfo(SharedInfo&&);

    SharedInfo& operator=(const SharedInfo& other);
    SharedInfo& operator=(SharedInfo&& other);

    std::string api = "private-model-training";
    std::string version = "1.0";
    base::Uuid report_id;
    url::Origin reporting_origin;
    base::Time scheduled_report_time;
  };

  cbor::Value::MapValue GetSharedInfoCborMap();

  SharedInfo shared_info_;
  auction_worklet::mojom::PrivateModelTrainingRequestDataPtr pmt_request_data_;
  BiddingAndAuctionServerKey public_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PMT_REPORT_UTIL_H_
