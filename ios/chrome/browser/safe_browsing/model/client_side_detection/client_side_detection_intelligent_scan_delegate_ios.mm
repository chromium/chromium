// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_intelligent_scan_delegate_ios.h"

#import <algorithm>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/json/values_util.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/model_execution/remote_model_executor.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/scam_detection.pb.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

namespace {
using ModelType = IntelligentScanDelegate::ModelType;
using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
using ScamDetectionResponse = optimization_guide::proto::ScamDetectionResponse;
}  // namespace

class ClientSideDetectionIntelligentScanDelegateIOS::Inquiry {
 public:
  Inquiry(ClientSideDetectionIntelligentScanDelegateIOS* parent,
          const base::UnguessableToken& scan_id,
          IntelligentScanDoneCallback callback);
  ~Inquiry();

  void Start(std::string rendered_texts);

 private:
  void RemoteExecutionCallback(
      base::TimeTicks remote_execution_start_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>);

  const raw_ptr<ClientSideDetectionIntelligentScanDelegateIOS> parent_ =
      nullptr;
  base::UnguessableToken scan_id_;
  IntelligentScanDoneCallback callback_;
  bool was_start_called_ = false;

  base::WeakPtrFactory<Inquiry> weak_factory_{this};
};

#pragma mark - Public

ClientSideDetectionIntelligentScanDelegateIOS::
    ClientSideDetectionIntelligentScanDelegateIOS(
        PrefService& pref,
        optimization_guide::RemoteModelExecutor* remote_model_executor)
    : pref_(pref),
      remote_model_executor_(remote_model_executor),
      is_feature_enabled_(
          !base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)),
      is_server_model_enabled_(base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelForScamDetectionIos)) {
  if (is_feature_enabled_) {
    pref_change_registrar_.Init(&pref);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnhanced,
        base::BindRepeating(
            &ClientSideDetectionIntelligentScanDelegateIOS::OnPrefsUpdated,
            base::Unretained(this)));
  }
}

ClientSideDetectionIntelligentScanDelegateIOS::
    ~ClientSideDetectionIntelligentScanDelegateIOS() = default;

#pragma mark - IntelligentScanDelegate

bool ClientSideDetectionIntelligentScanDelegateIOS::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  if (!verdict) {
    return false;
  }
  if (!is_feature_enabled_) {
    return false;
  }
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }
  if (verdict->client_side_detection_type() ==
          ClientSideDetectionType::IMAGE_EMBEDDING_MATCH &&
      verdict->is_phishing() &&
      kCsdImageEmbeddingMatchWithIntelligentScan.Get()) {
    return true;
  }
  return verdict->client_side_detection_type() ==
             ClientSideDetectionType::FORCE_REQUEST &&
         verdict->has_llama_forced_trigger_info() &&
         verdict->llama_forced_trigger_info().intelligent_scan();
}

IntelligentScanDelegate::ModelType
ClientSideDetectionIntelligentScanDelegateIOS::GetIntelligentScanModelType(
    bool /*log_failed_eligibility_reason*/) {
  if (!is_feature_enabled_) {
    return is_server_model_enabled_ ? ModelType::kNotSupportedServerSide
                                    : ModelType::kNotSupportedOnDevice;
  }
  if (is_server_model_enabled_) {
    return remote_model_executor_ ? ModelType::kServerSide
                                  : ModelType::kNotSupportedServerSide;
  }
  return ModelType::kNotSupportedOnDevice;
}

std::optional<base::UnguessableToken>
ClientSideDetectionIntelligentScanDelegateIOS::StartIntelligentScan(
    std::string rendered_texts,
    IntelligentScanDoneCallback callback) {
  ModelType model_type =
      GetIntelligentScanModelType(/*log_failed_eligibility_reason=*/false);
  if (!IntelligentScanDelegate::IsIntelligentScanAvailable(model_type)) {
    std::move(callback).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable, model_type,
        is_server_model_enabled_
            ? IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE
            : IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE));
    return std::nullopt;
  }

  bool is_at_quota = IsAtIntelligentScanQuota();
  if (is_server_model_enabled_) {
    base::UmaHistogramBoolean(
        "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", is_at_quota);
  }
  if (is_at_quota) {
    std::move(callback).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA));
    return std::nullopt;
  }

  base::UnguessableToken scan_id = base::UnguessableToken::Create();
  auto [it, inserted] = inquiries_.try_emplace(
      scan_id, std::make_unique<Inquiry>(this, scan_id, std::move(callback)));
  it->second->Start(std::move(rendered_texts));
  return scan_id;
}

bool ClientSideDetectionIntelligentScanDelegateIOS::CancelIntelligentScan(
    const base::UnguessableToken& scan_id) {
  return inquiries_.erase(scan_id) > 0;
}

bool ClientSideDetectionIntelligentScanDelegateIOS::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  if (!verdict.has_value() ||
      *verdict ==
          IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED ||
      *verdict == IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE ||
      *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY) {
    return false;
  }

  return *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_3 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_4 ||
         *verdict ==
             IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
}

void ClientSideDetectionIntelligentScanDelegateIOS::OnScamWarningShown() {
  if (!is_server_model_enabled_) {
    return;
  }

  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps).size());

  RemoveLastIntelligentScanQuota();
}

#pragma mark - KeyedService

void ClientSideDetectionIntelligentScanDelegateIOS::Shutdown() {
  ResetAllInquiries();
  remote_model_executor_ = nullptr;
  pref_change_registrar_.RemoveAll();
}

#pragma mark - Private

void ClientSideDetectionIntelligentScanDelegateIOS::OnPrefsUpdated() {
  if (!is_feature_enabled_) {
    return;
  }
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    ResetAllInquiries();
  }
}

bool ClientSideDetectionIntelligentScanDelegateIOS::ResetAllInquiries() {
  bool did_reset = !inquiries_.empty();
  inquiries_.clear();
  return did_reset;
}

bool ClientSideDetectionIntelligentScanDelegateIOS::IsAtIntelligentScanQuota() {
  if (!is_server_model_enabled_) {
    return false;
  }

  const base::Time now = base::Time::Now();
  auto is_expired = [now](const base::Value& timestamp_value) {
    constexpr base::TimeDelta kIntelligentScanQuotaInterval = base::Days(1);
    std::optional<base::Time> report_time = base::ValueToTime(timestamp_value);
    return !report_time.has_value() ||
           *report_time + kIntelligentScanQuotaInterval < now;
  };

  const base::ListValue& timestamps =
      pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  if (std::ranges::any_of(timestamps, is_expired)) {
    ScopedListPrefUpdate update(
        pref_.get(), prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
    update->EraseIf(is_expired);
  }

  // Clamp non-positive values to `0` to avoid unsigned wrap-around and block
  // all scans when the daily limit is configured to `<= 0`.
  return pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps)
             .size() >=
         static_cast<size_t>(std::max(
             0, kClientSideDetectionServerModelMaxScansPerDayIos.Get()));
}

void ClientSideDetectionIntelligentScanDelegateIOS::AddIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  update->Append(base::TimeToValue(base::Time::Now()));
  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnLookup", update->size());
}

void ClientSideDetectionIntelligentScanDelegateIOS::
    RemoveLastIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  base::UmaHistogramBoolean(
      "SBClientPhishing.ServerSideModelPrefEmptyWhenRemovingQuota",
      update->empty());
  if (!update->empty()) {
    update->erase(update->end() - 1);
  }
}

#pragma mark - Inquiry

ClientSideDetectionIntelligentScanDelegateIOS::Inquiry::Inquiry(
    ClientSideDetectionIntelligentScanDelegateIOS* parent,
    const base::UnguessableToken& scan_id,
    IntelligentScanDoneCallback callback)
    : parent_(parent), scan_id_(scan_id), callback_(std::move(callback)) {}

ClientSideDetectionIntelligentScanDelegateIOS::Inquiry::~Inquiry() = default;

void ClientSideDetectionIntelligentScanDelegateIOS::Inquiry::Start(
    std::string rendered_texts) {
  CHECK(!was_start_called_)
      << "Start() should only be called once per inquiry.";
  was_start_called_ = true;

  if (parent_->is_server_model_enabled_ && parent_->remote_model_executor_) {
    parent_->AddIntelligentScanQuota();
    ScamDetectionRequest request;
    request.set_rendered_text(std::move(rendered_texts));
    parent_->remote_model_executor_->ExecuteModel(
        optimization_guide::ModelBasedCapabilityKey::kScamDetection,
        std::move(request), /*options=*/{},
        base::BindOnce(&ClientSideDetectionIntelligentScanDelegateIOS::Inquiry::
                           RemoteExecutionCallback,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
    // Do not access `parent_` at this point. The callback may be called
    // synchronously and this object will delete itself.
    return;
  }

  // Move `callback_` to the stack before `CancelIntelligentScan()` destroys
  // `this`, and remove the inquiry before invoking `callback` so `this` is
  // never accessed after `callback` runs.
  auto callback = std::move(callback_);
  parent_->CancelIntelligentScan(scan_id_);
  std::move(callback).Run(IntelligentScanResult::Failure(
      IntelligentScanResult::kModelVersionUnavailable,
      ModelType::kNotSupportedServerSide,
      IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE));
}

void ClientSideDetectionIntelligentScanDelegateIOS::Inquiry::
    RemoteExecutionCallback(
        base::TimeTicks remote_execution_start_time,
        optimization_guide::OptimizationGuideModelExecutionResult result,
        std::unique_ptr<
            optimization_guide::ModelQualityLogEntry> /*log_entry*/) {
  CHECK(callback_);
  bool execution_success = result.response.has_value();
  base::UmaHistogramBoolean("SBClientPhishing.ServerSideModelExecutionSuccess",
                            execution_success);
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.ServerSideModelExecutionDuration",
      base::TimeTicks::Now() - remote_execution_start_time);

  int model_version =
      base::FeatureList::IsEnabled(kClientSideDetectionServerModelRolloutIos)
          ? kClientSideDetectionServerModelRolloutVersionIos.Get()
          : IntelligentScanResult::kDefaultServerModelVersion;

  if (!execution_success) {
    base::UmaHistogramEnumeration(
        "SBClientPhishing.ServerSideModelExecutionError",
        result.response.error().error());
    auto callback = std::move(callback_);
    parent_->CancelIntelligentScan(scan_id_);
    std::move(callback).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    return;
  }

  auto scam_detection_response =
      optimization_guide::ParsedAnyMetadata<ScamDetectionResponse>(
          result.response.value());

  if (!scam_detection_response) {
    auto callback = std::move(callback_);
    parent_->CancelIntelligentScan(scan_id_);
    std::move(callback).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    return;
  }

  std::optional<float> scam_score;
  if (scam_detection_response->has_scam_score()) {
    scam_score = scam_detection_response->scam_score();
  }

  // Move `callback_` to the stack before `CancelIntelligentScan()` destroys
  // `this`, and reset this inquiry immediately so that future inference is not
  // affected.
  auto callback = std::move(callback_);
  parent_->CancelIntelligentScan(scan_id_);
  std::move(callback).Run(IntelligentScanResult::Success(
      std::move(*scam_detection_response->mutable_brand()),
      std::move(*scam_detection_response->mutable_intent()), model_version,
      ModelType::kServerSide, scam_score));
}

}  // namespace safe_browsing
