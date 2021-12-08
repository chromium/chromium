// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// Converts `auction_config` back to JSON format, and appends to args.
// Returns true if conversion succeeded.
//
// The resulting object will look something like this (based on example from
// explainer):
//
// {
//  'seller': 'https://www.example-ssp.com/',
//  'decisionLogicUrl': 'https://www.example-ssp.com/seller.js',
//  'trustedScoringSignalsUrl': ...,
//  'interestGroupBuyers': ['https://www.example-dsp.com', 'https://buyer2.com',
//  ...], 'auctionSignals': {...}, 'sellerSignals': {...}, 'perBuyerSignals':
//  {'https://www.example-dsp.com': {...},
//                      'https://www.another-buyer.com': {...},
//                       ...}
// }
bool AppendAuctionConfig(AuctionV8Helper* const v8_helper,
                         v8::Local<v8::Context> context,
                         const blink::mojom::AuctionAdConfig& auction_config,
                         std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> auction_config_value = v8::Object::New(isolate);
  gin::Dictionary auction_config_dict(isolate, auction_config_value);
  if (!auction_config_dict.Set("seller", auction_config.seller.Serialize()) ||
      !auction_config_dict.Set("decisionLogicUrl",
                               auction_config.decision_logic_url.spec())) {
    return false;
  }

  if (auction_config.interest_group_buyers) {
    if (auction_config.interest_group_buyers->is_all_buyers()) {
      if (!auction_config_dict.Set("interestGroupBuyers", std::string("*")))
        return false;
    } else {
      std::vector<v8::Local<v8::Value>> interest_group_buyers;
      for (const url::Origin& buyer :
           auction_config.interest_group_buyers->get_buyers()) {
        v8::Local<v8::String> v8_buyer;
        if (!v8_helper->CreateUtf8String(buyer.Serialize()).ToLocal(&v8_buyer))
          return false;
        interest_group_buyers.push_back(v8_buyer);
      }
      auction_config_dict.Set("interestGroupBuyers", interest_group_buyers);
    }
  }

  if (auction_config.auction_signals.has_value() &&
      !v8_helper->InsertJsonValue(context, "auctionSignals",
                                  auction_config.auction_signals.value(),
                                  auction_config_value)) {
    return false;
  }

  if (auction_config.seller_signals.has_value() &&
      !v8_helper->InsertJsonValue(context, "sellerSignals",
                                  auction_config.seller_signals.value(),
                                  auction_config_value)) {
    return false;
  }

  if (auction_config.per_buyer_signals.has_value()) {
    v8::Local<v8::Object> per_buyer_value = v8::Object::New(isolate);
    for (const auto& kv : auction_config.per_buyer_signals.value()) {
      if (!v8_helper->InsertJsonValue(context, kv.first.Serialize(), kv.second,
                                      per_buyer_value)) {
        return false;
      }
    }
    auction_config_dict.Set("perBuyerSignals", per_buyer_value);
  }

  args->push_back(std::move(auction_config_value));
  return true;
}

}  // namespace

SellerWorklet::SellerWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    mojom::AuctionWorkletService::LoadSellerWorkletCallback
        load_worklet_callback)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(v8_helper),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper.get())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(script_source_url),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)),
      load_worklet_callback_(std::move(load_worklet_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(load_worklet_callback_);

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper_, debug_id_, script_source_url,
                  weak_ptr_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

SellerWorklet::~SellerWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (load_worklet_callback_) {
    std::move(load_worklet_callback_)
        .Run(false /* success */, std::vector<std::string>() /* errors */);
  }
  debug_id_->AbortDebuggerPauses();
}

int SellerWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

void SellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    blink::mojom::AuctionAdConfigPtr auction_config,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    ScoreAdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  score_ad_tasks_.emplace_front();

  auto score_ad_task = score_ad_tasks_.begin();
  score_ad_task->ad_metadata_json = ad_metadata_json;
  score_ad_task->bid = bid;
  score_ad_task->auction_config = std::move(auction_config);
  score_ad_task->browser_signal_top_window_origin =
      browser_signal_top_window_origin;
  score_ad_task->browser_signal_interest_group_owner =
      browser_signal_interest_group_owner;
  score_ad_task->browser_signal_render_url = browser_signal_render_url;
  for (const GURL& url : browser_signal_ad_components) {
    score_ad_task->browser_signal_ad_components.emplace_back(url.spec());
  }
  score_ad_task->browser_signal_bidding_duration_msecs =
      browser_signal_bidding_duration_msecs;
  score_ad_task->callback = std::move(callback);

  if (score_ad_task->auction_config->trusted_scoring_signals_url) {
    score_ad_task->trusted_scoring_signals = TrustedSignals::LoadScoringSignals(
        url_loader_factory_.get(),
        /*render_urls=*/
        std::vector<std::string>{browser_signal_render_url.spec()},
        score_ad_task->browser_signal_ad_components,
        browser_signal_top_window_origin.host(),
        *score_ad_task->auction_config->trusted_scoring_signals_url, v8_helper_,
        base::BindOnce(&SellerWorklet::OnTrustedScoringSignalsDownloaded,
                       base::Unretained(this), score_ad_task));
    return;
  }

  OnTrustedScoringSignalsDownloaded(score_ad_task, /*result=*/nullptr,
                                    /*error_msg=*/absl::nullopt);
}

void SellerWorklet::ReportResult(
    blink::mojom::AuctionAdConfigPtr auction_config,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_desirability,
    ReportResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ReportResult,
          base::Unretained(v8_state_.get()), std::move(auction_config),
          browser_signal_top_window_origin, browser_signal_interest_group_owner,
          browser_signal_render_url, browser_signal_bid,
          browser_signal_desirability, std::move(callback)));
}

void SellerWorklet::ConnectDevToolsAgent(
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
}

SellerWorklet::ScoreAdTask::ScoreAdTask() = default;
SellerWorklet::ScoreAdTask::~ScoreAdTask() = default;

SellerWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    GURL script_source_url,
    base::WeakPtr<SellerWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(debug_id),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunnerHandle::Get()),
      script_source_url_(std::move(script_source_url)) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this)));
}

void SellerWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = worklet_script.TakeScript();
}

void SellerWorklet::V8State::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::unique_ptr<TrustedSignals::Result> trusted_scoring_signals,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<std::string>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    ScoreAdCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!v8_helper_->AppendJsonValue(context, ad_metadata_json, &args)) {
    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::vector<std::string>() /* errors */);
    return;
  }

  args.push_back(gin::ConvertToV8(isolate, bid));

  if (!AppendAuctionConfig(v8_helper_.get(), context, *auction_config, &args)) {
    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::vector<std::string>() /* errors */);
    return;
  }

  v8::Local<v8::Value> trusted_scoring_signals_value;
  if (trusted_scoring_signals) {
    trusted_scoring_signals_value = trusted_scoring_signals->GetScoringSignals(
        v8_helper_.get(), context, browser_signal_render_url,
        browser_signal_ad_components);
  } else {
    trusted_scoring_signals_value = v8::Null(isolate);
  }
  args.push_back(trusted_scoring_signals_value);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_origin.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("biddingDurationMsec",
                                browser_signal_bidding_duration_msecs)) {
    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::vector<std::string>() /* errors */);
    return;
  }
  if (!browser_signal_ad_components.empty()) {
    if (!browser_signals_dict.Set("adComponents",
                                  browser_signal_ad_components)) {
      PostScoreAdCallbackToUserThread(std::move(callback), /*score=*/0,
                                      /*errors=*/std::vector<std::string>());
      return;
    }
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> score_ad_result;
  double score;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletScoringStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "scoreAd", args, errors_out)
           .ToLocal(&score_ad_result)) {
    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::move(errors_out));
    return;
  }

  if (!gin::ConvertFromV8(isolate, score_ad_result, &score) ||
      std::isnan(score) || !std::isfinite(score)) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " scoreAd() did not return a valid number."}));

    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::move(errors_out));
    return;
  }

  if (score <= 0) {
    PostScoreAdCallbackToUserThread(std::move(callback), 0 /* score */,
                                    std::move(errors_out));
    return;
  }

  PostScoreAdCallbackToUserThread(std::move(callback), score,
                                  std::move(errors_out));
}

void SellerWorklet::V8State::ReportResult(
    blink::mojom::AuctionAdConfigPtr auction_config,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_desirability,
    ReportResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ReportBindings report_bindings(v8_helper_.get(), global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendAuctionConfig(v8_helper_.get(), context, *auction_config, &args)) {
    PostReportResultCallbackToUserThread(
        std::move(callback), absl::nullopt /* signals_for_winner */,
        absl::nullopt /* report_url */,
        std::vector<std::string>() /* errors */);
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_origin.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set("desirability", browser_signal_desirability)) {
    PostReportResultCallbackToUserThread(
        std::move(callback), absl::nullopt /* signals_for_winner */,
        absl::nullopt /* report_url */,
        std::vector<std::string>() /* errors */);
    return;
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> signals_for_winner_value;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletReportingStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "reportResult", args, errors_out)
           .ToLocal(&signals_for_winner_value)) {
    PostReportResultCallbackToUserThread(
        std::move(callback), absl::nullopt /* signals_for_winner */,
        absl::nullopt /* report_url */, std::move(errors_out));
    return;
  }

  // Consider lack of error but no return value type, or a return value that
  // can't be converted to JSON a valid result.
  std::string signals_for_winner;
  if (!v8_helper_->ExtractJson(context, signals_for_winner_value,
                               &signals_for_winner)) {
    signals_for_winner = "null";
  }

  PostReportResultCallbackToUserThread(
      std::move(callback), std::move(signals_for_winner),
      report_bindings.report_url(), std::move(errors_out));
}

void SellerWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

SellerWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void SellerWorklet::V8State::FinishInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  debug_id_->SetResumeCallback(base::BindOnce(
      &SellerWorklet::V8State::PostResumeToUserThread, parent_, user_thread_));
}

// static
void SellerWorklet::V8State::PostResumeToUserThread(
    base::WeakPtr<SellerWorklet> parent,
    scoped_refptr<base::SequencedTaskRunner> user_thread) {
  // This is static since it's called from debugging, not SellerWorklet,
  // so the usual guarantee that SellerWorklet posts things before posting
  // V8State destruction is irrelevant.
  user_thread->PostTask(FROM_HERE,
                        base::BindOnce(&SellerWorklet::ResumeIfPaused, parent));
}

void SellerWorklet::V8State::PostScoreAdCallbackToUserThread(
    ScoreAdCallbackInternal callback,
    double score,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), score, std::move(errors)));
}

void SellerWorklet::V8State::PostReportResultCallbackToUserThread(
    ReportResultCallback callback,
    absl::optional<std::string> signals_for_winner,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // `parent` being a weak pointer takes care of the case where the
  // SellerWorklet proper is destroyed.
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&SellerWorklet::DeliverReportResultCallbackOnUserThread,
                     parent_, std::move(callback),
                     std::move(signals_for_winner), std::move(report_url),
                     std::move(errors)));
}

void SellerWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_)
    return;

  paused_ = false;
  Start();
}

void SellerWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&SellerWorklet::OnDownloadComplete,
                     base::Unretained(this)));
}

void SellerWorklet::OnDownloadComplete(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(load_worklet_callback_);
  worklet_loader_.reset();

  bool success = worklet_script.success();
  if (success) {
    v8_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SellerWorklet::V8State::SetWorkletScript,
                                  base::Unretained(v8_state_.get()),
                                  std::move(worklet_script)));
  }

  std::vector<std::string> errors;
  if (error_msg)
    errors.emplace_back(std::move(error_msg).value());
  std::move(load_worklet_callback_).Run(success, errors);
}

void SellerWorklet::OnTrustedScoringSignalsDownloaded(
    ScoreAdTaskList::iterator task,
    std::unique_ptr<TrustedSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_scoring_signals_error_msg = std::move(error_msg);
  // Clean up single-use object.
  task->trusted_scoring_signals.reset();

  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ScoreAd, base::Unretained(v8_state_.get()),
          task->ad_metadata_json, task->bid, std::move(task->auction_config),
          std::move(result), std::move(task->browser_signal_top_window_origin),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_ad_components),
          task->browser_signal_bidding_duration_msecs,
          base::BindOnce(&SellerWorklet::DeliverScoreAdCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverScoreAdCallbackOnUserThread(
    ScoreAdTaskList::iterator task,
    double score,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (task->trusted_scoring_signals_error_msg)
    errors.insert(errors.begin(), *task->trusted_scoring_signals_error_msg);

  std::move(task->callback).Run(score, std::move(errors));
  score_ad_tasks_.erase(task);
}

void SellerWorklet::DeliverReportResultCallbackOnUserThread(
    ReportResultCallback callback,
    absl::optional<std::string> signals_for_winner,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  std::move(callback).Run(std::move(signals_for_winner), std::move(report_url),
                          std::move(errors));
}

}  // namespace auction_worklet
