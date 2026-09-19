// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/prefs.h"
#import "components/universal_optout/universal_optout_service.h"
#import "ios/web/public/extension/extension_controller.h"

namespace {

constexpr base::TimeDelta kExtensionLoadingTimeout = base::Seconds(2);

}  // namespace

ExtensionService::ExtensionService(
    PrefService* pref_service,
    universal_optout::UniversalOptOutService* universal_optout_service,
    std::unique_ptr<web::ExtensionController> extension_controller)
    API_AVAILABLE(ios(18.4))
    : pref_service_(pref_service),
      universal_optout_service_(universal_optout_service),
      extension_controller_(std::move(extension_controller)) {}

ExtensionService::~ExtensionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExtensionService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  loading_timer_.Stop();
  pref_change_registrar_.RemoveAll();
  ready_callbacks_.clear();
  extension_controller_.reset();
  universal_optout_service_ = nullptr;
  pref_service_ = nullptr;
}

web::ExtensionController* ExtensionService::GetExtensionController() const
    API_AVAILABLE(ios(18.4)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return extension_controller_.get();
}

bool ExtensionService::IsReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_ready_;
}

void ExtensionService::RunWhenReady(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_ready_) {
    std::move(callback).Run();
    return;
  }
  ready_callbacks_.push_back(std::move(callback));
}

void ExtensionService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_eligible =
      universal_optout_service_ && universal_optout_service_->IsEligible();

  if (!is_eligible || !extension_controller_) {
    is_ready_ = true;
    return;
  }

  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        universal_optout::prefs::kUniversalOptOutEnabled,
        base::BindRepeating(&ExtensionService::OnOptOutPrefChanged,
                            base::Unretained(this)));
  }

  const bool opted_in =
      pref_service_ && pref_service_->GetBoolean(
                           universal_optout::prefs::kUniversalOptOutEnabled);
  if (opted_in) {
    is_loading_ = true;
    loading_timer_.Start(
        FROM_HERE, kExtensionLoadingTimeout,
        base::BindOnce(&ExtensionService::OnExtensionLoadTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
    extension_controller_->LoadBuiltInExtension(
        web::BuiltInExtension::kGPC,
        base::BindOnce(&ExtensionService::OnExtensionLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    is_ready_ = true;
  }
}

void ExtensionService::OnExtensionLoaded(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  loading_timer_.Stop();
  if (pref_service_ && !pref_service_->GetBoolean(
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

void ExtensionService::OnExtensionLoadTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_ = false;
  if (is_ready_) {
    return;
  }
  NotifyReady();
}

void ExtensionService::NotifyReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_ready_ = true;
  std::vector<base::OnceClosure> callbacks = std::move(ready_callbacks_);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

void ExtensionService::OnOptOutPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!extension_controller_ || !pref_service_) {
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
          base::BindOnce(&ExtensionService::OnExtensionLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else if (is_loaded) {
    extension_controller_->UnloadBuiltInExtension(web::BuiltInExtension::kGPC,
                                                  base::DoNothing());
  }
}
