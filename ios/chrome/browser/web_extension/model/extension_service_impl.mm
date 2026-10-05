// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service_impl.h"

#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/web_client.h"

namespace {

constexpr base::TimeDelta kExtensionLoadingTimeout = base::Seconds(2);

}  // namespace

ExtensionServiceImpl::ExtensionServiceImpl(
    PrefService& pref_service,
    std::unique_ptr<web::ExtensionController> extension_controller)
    : pref_service_(pref_service),
      extension_controller_(std::move(extension_controller)) {}

ExtensionServiceImpl::~ExtensionServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExtensionServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  has_load_error_ = false;
  loading_timer_.Stop();
  pref_change_registrar_.RemoveAll();
  ready_callbacks_.Clear();
  observers_.Clear();
  extension_controller_.reset();
  extension_load_start_time_ = base::TimeTicks();
  initialization_start_time_ = base::TimeTicks();
}

void ExtensionServiceImpl::Initialize(web::UniversalOptOutState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!extension_controller_) {
    is_ready_ = true;
    return;
  }

  const bool is_silent =
      universal_optout::features::IsUniversalOptOutExtensionSilentEnabled();

  if (!is_silent && state == web::UniversalOptOutState::kNotEligible) {
    is_ready_ = true;
    return;
  }

  pref_change_registrar_.Init(&pref_service_.get());
  pref_change_registrar_.Add(
      universal_optout::prefs::kUniversalOptOutEnabled,
      base::BindRepeating(&ExtensionServiceImpl::OnOptOutPrefChanged,
                          base::Unretained(this)));

  const bool should_load_silent_extension =
      is_silent &&
      !universal_optout::features::kUniversalOptOutExtensionSilentControllerOnly
           .Get();

  const bool should_load_at_startup =
      should_load_silent_extension ||
      (!is_silent && state == web::UniversalOptOutState::kEnabled);

  if (should_load_at_startup) {
    extension_load_started_at_startup_ = true;
    is_loading_ = true;
    initialization_start_time_ = base::TimeTicks::Now();
    extension_load_start_time_ = initialization_start_time_;
    loading_timer_.Start(
        FROM_HERE, kExtensionLoadingTimeout,
        base::BindOnce(&ExtensionServiceImpl::OnExtensionLoadTimeout,
                       weak_ptr_factory_.GetWeakPtr()));

    web::BuiltInExtension extension_to_load =
        is_silent ? web::BuiltInExtension::kGPCTest
                  : web::BuiltInExtension::kGPC;

    extension_controller_->LoadBuiltInExtension(
        extension_to_load,
        base::BindOnce(&ExtensionServiceImpl::OnExtensionLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    is_ready_ = true;
  }
}

web::ExtensionController* ExtensionServiceImpl::GetExtensionController() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return extension_controller_.get();
}

void ExtensionServiceImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ExtensionServiceImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool ExtensionServiceImpl::HasLoadError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_load_error_;
}

void ExtensionServiceImpl::SetHasLoadError(bool has_load_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_load_error_ == has_load_error) {
    return;
  }
  has_load_error_ = has_load_error;
  for (Observer& observer : observers_) {
    observer.OnExtensionLoadErrorChanged(this, has_load_error_);
  }
}

bool ExtensionServiceImpl::IsReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_ready_;
}

bool ExtensionServiceImpl::WebExtensionsWereLoadedAtStartup() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return extension_load_started_at_startup_;
}

base::CallbackListSubscription ExtensionServiceImpl::RunWhenReady(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_ready_);
  return ready_callbacks_.Add(std::move(callback));
}

void ExtensionServiceImpl::OnExtensionLoaded(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  loading_timer_.Stop();
  if (!extension_load_start_time_.is_null()) {
    base::UmaHistogramBoolean("IOS.WebExtension.LoadSuccess", success);
    base::UmaHistogramTimes(
        "IOS.WebExtension.LoadDelay",
        base::TimeTicks::Now() - extension_load_start_time_);
    extension_load_start_time_ = base::TimeTicks();
  }
  const bool is_silent =
      universal_optout::features::IsUniversalOptOutExtensionSilentEnabled();
  const bool is_opt_out_enabled =
      is_silent || pref_service_->GetBoolean(
                       universal_optout::prefs::kUniversalOptOutEnabled);
  if (!is_opt_out_enabled || success) {
    SetHasLoadError(false);
  } else {
    SetHasLoadError(true);
  }
  if (!is_silent && !pref_service_->GetBoolean(
                        universal_optout::prefs::kUniversalOptOutEnabled)) {
    if (extension_controller_ &&
        extension_controller_->IsBuiltInExtensionLoaded(
            web::BuiltInExtension::kGPC)) {
      extension_controller_->UnloadBuiltInExtension(web::BuiltInExtension::kGPC,
                                                    base::DoNothing());
    }
  }
  if (is_ready_) {
    return;
  }
  NotifyReady();
}

void ExtensionServiceImpl::OnExtensionLoadTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  const bool is_silent =
      universal_optout::features::IsUniversalOptOutExtensionSilentEnabled();
  const bool is_opt_out_enabled =
      is_silent || pref_service_->GetBoolean(
                       universal_optout::prefs::kUniversalOptOutEnabled);
  if (is_opt_out_enabled) {
    SetHasLoadError(true);
  }
  if (is_ready_) {
    return;
  }
  NotifyReady();
}

void ExtensionServiceImpl::NotifyReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initialization_start_time_.is_null()) {
    base::UmaHistogramTimes(
        "IOS.WebExtension.ReadyDelay",
        base::TimeTicks::Now() - initialization_start_time_);
    initialization_start_time_ = base::TimeTicks();
  }
  is_ready_ = true;
  ready_callbacks_.Notify();
}

void ExtensionServiceImpl::OnOptOutPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (universal_optout::features::IsUniversalOptOutExtensionSilentEnabled()) {
    return;
  }
  if (!extension_controller_) {
    return;
  }
  const bool enabled = pref_service_->GetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled);
  const bool is_loaded = extension_controller_->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC);
  if (enabled) {
    if (!is_loaded && !is_loading_) {
      is_loading_ = true;
      extension_controller_->LoadBuiltInExtension(
          web::BuiltInExtension::kGPC,
          base::BindOnce(&ExtensionServiceImpl::OnExtensionLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    SetHasLoadError(false);
    if (is_loaded) {
      extension_controller_->UnloadBuiltInExtension(web::BuiltInExtension::kGPC,
                                                    base::DoNothing());
    }
  }
}
