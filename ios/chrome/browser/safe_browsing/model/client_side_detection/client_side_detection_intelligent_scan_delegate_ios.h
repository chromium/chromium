// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_IOS_H_

#import <cstddef>
#import <memory>
#import <optional>
#import <string>

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/unguessable_token.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"

class PrefService;

namespace optimization_guide {
class RemoteModelExecutor;
}  // namespace optimization_guide

namespace safe_browsing {

// Client Side Detection iOS implementation of `IntelligentScanDelegate`. This
// class manages intelligent scan inquiries and executes the server-side model
// via the OptimizationGuide `RemoteModelExecutor`.
class ClientSideDetectionIntelligentScanDelegateIOS
    : public IntelligentScanDelegate {
 public:
  // Constructs an instance using the provided `pref` and
  // `remote_model_executor`.
  ClientSideDetectionIntelligentScanDelegateIOS(
      PrefService& pref,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  ~ClientSideDetectionIntelligentScanDelegateIOS() override;

  ClientSideDetectionIntelligentScanDelegateIOS(
      const ClientSideDetectionIntelligentScanDelegateIOS&) = delete;
  ClientSideDetectionIntelligentScanDelegateIOS& operator=(
      const ClientSideDetectionIntelligentScanDelegateIOS&) = delete;

  // IntelligentScanDelegate implementation:
  // Determines if an intelligent scan should be requested based on the verdict.
  bool ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) override;

  // Returns the model type that the client uses to perform intelligent scan.
  // `log_failed_eligibility_reason` is unused because failed eligibility reason
  // metrics are specific to the on-device model, which is not supported on iOS.
  ModelType GetIntelligentScanModelType(
      bool /*log_failed_eligibility_reason*/) override;

  // Starts an intelligent scan for the provided `rendered_texts` and invokes
  // `callback` with the result upon completion. Returns an unguessable token
  // identifying the request, or `std::nullopt` if the request could not start.
  std::optional<base::UnguessableToken> StartIntelligentScan(
      std::string rendered_texts,
      IntelligentScanDoneCallback callback) override;

  // Cancels a specific intelligent scan identified by `scan_id`. Returns `true`
  // if an active inquiry was cancelled, `false` otherwise.
  bool CancelIntelligentScan(const base::UnguessableToken& scan_id) override;

  // Determines if a CSD scam warning should be shown based on `verdict`.
  bool ShouldShowScamWarning(
      std::optional<IntelligentScanVerdict> verdict) override;

  // Called when a scam warning interstitial is shown to the user to refund
  // the consumed scan quota.
  void OnScamWarningShown() override;

  // KeyedService implementation:
  // Shuts down the delegate, invalidating active inquiries and releasing
  // references to dependencies.
  void Shutdown() override;

  // Returns the number of currently active inquiries. Used for testing.
  size_t GetAliveInquiryCountForTesting() const { return inquiries_.size(); }

 private:
  class Inquiry;

  // Invoked when Safe Browsing preferences change.
  void OnPrefsUpdated();

  // Resets and cancels all active inquiries. Returns `true` if there were
  // active inquiries.
  bool ResetAllInquiries();

  // Returns `true` if the daily quota limit for intelligent scans has been
  // reached. Prunes timestamps older than 24 hours.
  bool IsAtIntelligentScanQuota();

  // Records a scan timestamp in prefs for quota tracking.
  void AddIntelligentScanQuota();

  // Removes the most recent scan timestamp from prefs to refund quota.
  void RemoveLastIntelligentScanQuota();

  // Reference to profile prefs.
  const raw_ref<PrefService> pref_;

  // Service used for server-side model execution. May be `nullptr` after
  // shutdown.
  raw_ptr<optimization_guide::RemoteModelExecutor> remote_model_executor_ =
      nullptr;

  // Active intelligent scan inquiries indexed by scan token.
  base::flat_map<base::UnguessableToken, std::unique_ptr<Inquiry>> inquiries_;

  // Registrar used to observe Enhanced Safe Browsing preference updates.
  PrefChangeRegistrar pref_change_registrar_;

  const bool is_feature_enabled_;
  const bool is_server_model_enabled_;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_IOS_H_
