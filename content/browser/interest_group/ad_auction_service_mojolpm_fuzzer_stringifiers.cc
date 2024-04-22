// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer_stringifiers.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "content/browser/interest_group/ad_auction_service_mojolpm_fuzzer_stringifiers.pb.h"

namespace content::ad_auction_service_mojolpm_fuzzer {

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Script& script) {
  if (script.has_raw_script()) {
    return script.raw_script();
  }
  return Stringify(script.constructed_script());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ConstructedScript&
        script) {
  std::string output;
  if (script.has_generate_bid_function()) {
    output += Stringify(script.generate_bid_function());
  }
  if (script.has_score_ad_function()) {
    output += Stringify(script.score_ad_function());
  }
  if (script.has_report_win_function()) {
    output += Stringify(script.report_win_function());
  }
  if (script.has_report_result_function()) {
    output += Stringify(script.report_result_function());
  }
  return output;
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidFunction&
        generate_bid) {
  std::string output = R"(function generateBid(
interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
browserSignals, directFromSellerSignals) {
)";
  for (const auto& call_set_bid : generate_bid.call_set_bid()) {
    output += Stringify(call_set_bid);
  }
  for (const auto& call_debug_only_report_win :
       generate_bid.call_debug_only_report_win()) {
    output += Stringify(call_debug_only_report_win);
  }
  for (const auto& call_debug_only_report_loss :
       generate_bid.call_debug_only_report_loss()) {
    output += Stringify(call_debug_only_report_loss);
  }
  if (generate_bid.has_generate_bid_return()) {
    output += Stringify(generate_bid.generate_bid_return());
  }
  output += "}\n\n";
  return output;
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ScoreAdFunction&
        score_ad) {
  // TODO(crbug.com/41490389): Flush out.
  return
      R"(function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
browserSignals) {
return {
desirability: bid,
allowComponentAuction: true
};
}

)";
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ReportWinFunction&
        report_win) {
  // TODO(crbug.com/41490389): Flush out.
  return
      R"(function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
browserSignals, directFromSellerSignals) {
}

)";
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ReportResultFunction&
        report_result) {
  // TODO(crbug.com/41490389): Flush out.
  return
      R"(function reportResult(auctionConfig, browserSignals) {
return {};
}

)";
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::CallSetBid&
        call_set_bid) {
  return base::StringPrintf(
      "setBid(%s);\n",
      Stringify(call_set_bid.generate_bid_return_value()).c_str());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::CallDebugOnlyReportWin&
        debug_only_report_win) {
  return base::StringPrintf("forDebuggingOnly.reportAdAuctionWin(%s);\n",
                            Stringify(debug_only_report_win.url()).c_str());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::CallDebugOnlyReportLoss&
        debug_only_report_loss) {
  return base::StringPrintf("forDebuggingOnly.reportAdAuctionLoss(%s);\n",
                            Stringify(debug_only_report_loss.url()).c_str());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturn&
        generate_bid_return) {
  return base::StringPrintf(
      "return %s;\n",
      Stringify(generate_bid_return.generate_bid_return_value()).c_str());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturnValue&
        generate_bid_return_value) {
  if (generate_bid_return_value.has_raw_return()) {
    return Stringify(generate_bid_return_value.raw_return());
  } else if (generate_bid_return_value.has_generate_bid_return_dict()) {
    return Stringify(generate_bid_return_value.generate_bid_return_dict());
  } else if (generate_bid_return_value.has_generate_bid_return_dict_array()) {
    return Stringify(
        generate_bid_return_value.generate_bid_return_dict_array());
  }
  return "";
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::GenerateBidReturnDict&
        generate_bid_return_dict) {
  std::string output = "{\n";
  if (generate_bid_return_dict.has_ad()) {
    output += base::StringPrintf(
        "'ad': %s,\n", Stringify(generate_bid_return_dict.ad()).c_str());
  }
  if (generate_bid_return_dict.has_ad_cost()) {
    output += base::StringPrintf(
        "'adCost': %s,\n",
        Stringify(generate_bid_return_dict.ad_cost()).c_str());
  }
  if (generate_bid_return_dict.has_bid()) {
    output += base::StringPrintf(
        "'bid': %s,\n", Stringify(generate_bid_return_dict.bid()).c_str());
  }
  if (generate_bid_return_dict.has_bid_currency()) {
    output += base::StringPrintf(
        "'bidCurrency': %s,\n",
        Stringify(generate_bid_return_dict.bid_currency()).c_str());
  }
  if (generate_bid_return_dict.has_render()) {
    output += base::StringPrintf(
        "'render': %s,\n",
        Stringify(generate_bid_return_dict.render()).c_str());
  }
  if (!generate_bid_return_dict.ad_components().empty()) {
    output += "'adComponents': [\n";
    for (const auto& ad_component : generate_bid_return_dict.ad_components()) {
      output += base::StringPrintf("%s,\n", Stringify(ad_component).c_str());
    }
    output += "],\n";
  }
  if (generate_bid_return_dict.has_allow_component_auction()) {
    output += base::StringPrintf(
        "'allowComponentAuction': %s,\n",
        Stringify(generate_bid_return_dict.allow_component_auction()).c_str());
  }
  if (generate_bid_return_dict.has_modeling_signals()) {
    output += base::StringPrintf(
        "'modelingSignals': %s,\n",
        Stringify(generate_bid_return_dict.modeling_signals()).c_str());
  }
  output += "}";
  return output;
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::
        GenerateBidReturnDictArray& generate_bid_return_dict_array) {
  std::string output = "[";
  for (const auto& return_dict :
       generate_bid_return_dict_array.generate_bid_return_dict()) {
    output += Stringify(return_dict) + ",\n";
  }
  return output + "]";
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Render& render) {
  return render.has_value() ? Stringify(render.value())
                            : Stringify(render.structured_render());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::StructuredRender&
        structured_render) {
  std::string output = "{\n";
  if (structured_render.has_url()) {
    output += base::StringPrintf("'url': %s,\n",
                                 Stringify(structured_render.url()).c_str());
  }
  if (structured_render.has_width()) {
    output += base::StringPrintf("'width': %s,\n",
                                 Stringify(structured_render.width()).c_str());
  }
  if (structured_render.has_height()) {
    output += base::StringPrintf("'height': %s,\n",
                                 Stringify(structured_render.height()).c_str());
  }
  output += "}";
  return output;
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Dimension& dimension) {
  if (dimension.has_value()) {
    return Stringify(dimension.value());
  } else if (dimension.has_structured_dimension()) {
    return Stringify(dimension.structured_dimension());
  } else {
    return "";
  }
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::StructuredDimension&
        structured_dimension) {
  std::string unit;
  if (structured_dimension.has_unit_sw()) {
    unit = "sw";
  } else if(structured_dimension.has_unit_px()) {
    unit = "px";
  }
  return base::StringPrintf("%s + '%s'",
                            Stringify(structured_dimension.value()).c_str(),
                            unit.c_str());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Value& value) {
  return value.has_raw_value() ? value.raw_value()
                               : Stringify(value.param_value());
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::ParamValue&
        param_value) {
  if (param_value.has_interest_group()) {
    return Stringify(param_value.interest_group());
  } else if (param_value.has_auction_signals()) {
    return "auctionSignals";
  } else if (param_value.has_per_buyer_signals()) {
    return "perBuyerSignals";
  } else if (param_value.has_trusted_bidding_signals()) {
    return "trustedBiddingSignals";
  } else if (param_value.has_browser_signals()) {
    return Stringify(param_value.browser_signals());
  } else if (param_value.has_direct_from_seller_signals()) {
    return Stringify(param_value.direct_from_seller_signals());
  } else {
    return "";
  }
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::InterestGroup&
        interest_group) {
  constexpr char kPrefix[] = "interestGroup.";
  if (interest_group.has_owner()) {
    return base::StrCat({kPrefix, "owner"});
  } else if (interest_group.has_name()) {
    return base::StrCat({kPrefix, "name"});
  } else if (interest_group.has_enable_bidding_signals_prioritization()) {
    return base::StrCat({kPrefix, "enableBiddingSignalsPrioritization"});
  } else if (interest_group.has_execution_mode()) {
    return base::StrCat({kPrefix, "executionMode"});
  } else if (interest_group.has_trusted_bidding_signals_slot_size_mode()) {
    return base::StrCat({kPrefix, "trustedBiddingSignalsSlotSizeMode"});
  } else if (interest_group.has_user_bidding_signals()) {
    return base::StrCat({kPrefix, "userBiddingSignals"});
  } else if (interest_group.has_bidding_logic_url()) {
    return base::StrCat({kPrefix, "biddingLogicURL"});
  } else if (interest_group.has_bidding_logic_url_deprecated()) {
    return base::StrCat({kPrefix, "biddingLogicUrl"});
  } else if (interest_group.has_bidding_wasm_helper_url()) {
    return base::StrCat({kPrefix, "biddingWasmHelperURL"});
  } else if (interest_group.has_bidding_wasm_helper_url_deprecated()) {
    return base::StrCat({kPrefix, "biddingWasmHelperUrl"});
  } else if (interest_group.has_update_url()) {
    return base::StrCat({kPrefix, "updateURL"});
  } else if (interest_group.has_update_url_deprecated()) {
    return base::StrCat({kPrefix, "updateUrl"});
  } else if (interest_group.has_daily_update_url()) {
    return base::StrCat({kPrefix, "dailyUpdateUrl"});
  } else if (interest_group.has_trusted_bidding_signals_url()) {
    return base::StrCat({kPrefix, "trustedBiddingSignalsURL"});
  } else if (interest_group.has_trusted_bidding_signals_url_deprecated()) {
    return base::StrCat({kPrefix, "trustedBiddingSignalsUrl"});
  } else if (interest_group.has_trusted_bidding_signals_keys()) {
    return base::StrCat({kPrefix, "trustedBiddingSignalsKeys"});
  } else if (interest_group.has_priority_vector()) {
    return base::StrCat({kPrefix, "priorityVector"});
  } else if (interest_group.has_use_bidding_signals_prioritization()) {
    return base::StrCat({kPrefix, "useBiddingSignalsPrioritization"});
  } else if (interest_group.has_ads()) {
    return base::StrCat({kPrefix, Stringify(interest_group.ads())});
  } else if (interest_group.has_ad_components()) {
    return base::StrCat({kPrefix, Stringify(interest_group.ad_components())});
  } else {
    return "";
  }
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::Ads& ads) {
  constexpr char kPrefix[] = "ads";
  return ads.has_ad() ? base::StrCat({kPrefix, Stringify(ads.ad())})
                      : std::string(kPrefix);
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdOrAdComponent& aoac) {
  return base::StringPrintf(
      "[%d]%s", aoac.index(),
      aoac.has_ad_field() ? Stringify(aoac.ad_field()).c_str() : "");
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdField& ad_field) {
  constexpr char kPrefix[] = ".";
  if (ad_field.has_render_url()) {
    return base::StrCat({kPrefix, "renderURL"});
  } else if (ad_field.has_render_url_deprecated()) {
    return base::StrCat({kPrefix, "renderUrl"});
  } else {
    return base::StrCat({kPrefix, "metadata"});
  }
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::AdComponents&
        ad_components) {
  constexpr char kPrefix[] = "adComponents";
  return ad_components.has_ad()
             ? base::StrCat({kPrefix, Stringify(ad_components.ad())})
             : std::string(kPrefix);
}

std::string Stringify(const content::fuzzing::ad_auction_service::proto::
                          GenerateBidBrowserSignals& browser_signals) {
  constexpr char kPrefix[] = "browserSignals.";
  if (browser_signals.has_top_window_hostname()) {
    return base::StrCat({kPrefix, "topWindowHostname"});
  } else if (browser_signals.has_seller()) {
    return base::StrCat({kPrefix, "seller"});
  } else if (browser_signals.has_top_level_seller()) {
    return base::StrCat({kPrefix, "topLevelSeller"});
  } else if (browser_signals.has_requested_size()) {
    return base::StrCat({kPrefix, "requestedSize"});
  } else if (browser_signals.has_join_count()) {
    return base::StrCat({kPrefix, "joinCount"});
  } else if (browser_signals.has_recency()) {
    return base::StrCat({kPrefix, "recency"});
  } else if (browser_signals.has_bid_count()) {
    return base::StrCat({kPrefix, "bidCount"});
  } else if (browser_signals.has_prev_wins()) {
    return base::StrCat({kPrefix, "prevWins"});
  } else if (browser_signals.has_prev_wins_ms()) {
    return base::StrCat({kPrefix, "prevWinsMs"});
  } else if (browser_signals.has_wasm_helper()) {
    return base::StrCat({kPrefix, "wasmHelper"});
  } else if (browser_signals.has_data_version()) {
    return base::StrCat({kPrefix, "dataVersion"});
  } else if (browser_signals.has_ad_components_limit()) {
    return base::StrCat({kPrefix, "adComponentsLimit"});
  } else {
    return base::StrCat({kPrefix, "multiBidLimit"});
  }
}

std::string Stringify(
    const content::fuzzing::ad_auction_service::proto::DirectFromSellerSignals&
        direct_from_seller_signals) {
  constexpr char kPrefix[] = "directFromSellerSignals.";
  if (direct_from_seller_signals.has_auction_signals()) {
    return base::StrCat({kPrefix, "auctionSignals"});
  } else if (direct_from_seller_signals.has_per_buyer_signals()) {
    return base::StrCat({kPrefix, "perBuyerSignals"});
  } else {
    return base::StrCat({kPrefix, "sellerSignals"});
  }
}

}  // namespace content::ad_auction_service_mojolpm_fuzzer
