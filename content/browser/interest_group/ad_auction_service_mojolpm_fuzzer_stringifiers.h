// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_MOJOLPM_FUZZER_STRINGIFIERS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_MOJOLPM_FUZZER_STRINGIFIERS_H_

#include <string>

#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer_stringifiers.pb.h"

namespace content::ad_auction_service_mojolpm_fuzzer {

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Script&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ConstructedScript&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidFunction&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ScoreAdFunction&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ReportWinFunction&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ReportResultFunction&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::CallSetBid&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::CallDebugOnlyReportWin&);

std::string Stringify(const content::fuzzing::ad_auction_service::proto::
                          CallDebugOnlyReportLoss&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturn&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturnValue&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturnDict&);

std::string Stringify(const content::fuzzing::ad_auction_service::proto::
                          GenerateBidReturnDictArray&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Render&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::StructuredRender&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Dimension&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::StructuredDimension&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Value&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ParamValue&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::InterestGroup&);

std::string Stringify(const content::fuzzing::ad_auction_service::proto::Ads&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdOrAdComponent&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdField&);

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdComponents&);

std::string Stringify(const content::fuzzing::ad_auction_service::proto::
                          GenerateBidBrowserSignals&);

std::string Stringify(const content::fuzzing::ad_auction_service::proto::
                          DirectFromSellerSignals&);

}  // namespace content::ad_auction_service_mojolpm_fuzzer

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_MOJOLPM_FUZZER_STRINGIFIERS_H_
