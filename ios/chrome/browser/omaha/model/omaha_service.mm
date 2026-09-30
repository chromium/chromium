// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_service.h"

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/no_destructor.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/values.h"
#import "build/branding_buildflags.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/omaha/model/omaha_backend.h"
#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace {

// Returns whether Omaha is enabled for this build variant.
bool IsOmahaServiceEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return !tests_hook::DisableUpdateService();
#else
  return false;
#endif
}

}  // namespace

OmahaService::OmahaService() {}

OmahaService::OmahaService(const PrefService& local_state,
                           const base::i18n::LanguageTag& language_tag) {
  if (IsOmahaServiceEnabled()) {
    GURL omaha_server_url = ios::provider::GetOmahaUpdateServerURL();
    if (omaha_server_url.is_valid()) {
      const base::Time app_install = base::Time::FromTimeT(
          local_state.GetInt64(metrics::prefs::kInstallDate));

      backend_.emplace(web::GetIOThreadTaskRunner({}),
                       /*locale_lang=*/std::string(language_tag.tag_string()),
                       /*app_install=*/app_install,
                       /*omaha_server_url=*/std::move(omaha_server_url),
                       /*auto_schedule=*/true);
    }
  }
}

OmahaService::~OmahaService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OmahaService::Start(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    UpgradeRecommendedCallback upgrade_recommended_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!started_);

  started_ = true;
  if (backend_.is_null()) {
    // If the backend has not been initialized, it means either the service
    // is disabled or the server URL is invalid. In all case, do nothing.
    return;
  }

  // Save the callback.
  upgrade_recommended_callback_ = std::move(upgrade_recommended_callback);

  // Since the backend lives on a background sequence and invokes the callback
  // in the sequence it is bound to, use base::BindPostTask(...) to ensure the
  // callbacks run on the correct sequence.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  backend_.AsyncCall(&OmahaBackend::Start)
      .WithArgs(
          OmahaPersistentState::LoadFrom(defaults),
          base::BindOnce(&network::SharedURLLoaderFactory::Create,
                         shared_url_loader_factory->Clone()),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(&OmahaService::OnPingReceived,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(&OmahaPersistentState::SaveTo, defaults)));
}

bool OmahaService::HasStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return started_ && !backend_.is_null();
}

void OmahaService::CheckNow(OneOffCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasStarted()) {
    // If the backend has not been initialized, or the service not started,
    // pretend the server has responded that the application is up to date.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       UpgradeRecommendedDetails{.is_up_to_date = true}));
    return;
  }

  DCHECK(!one_off_callback_);
  one_off_callback_ = std::move(callback);
  backend_.AsyncCall(&OmahaBackend::CheckNow);
}

void OmahaService::GetDebugInformation(
    base::OnceCallback<void(base::DictValue)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasStarted()) {
    // If the backend has not been initialized, there is no debug info.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::DictValue{}));
    return;
  }

  backend_.AsyncCall(&OmahaBackend::GetDebugInformation)
      .Then(std::move(callback));
}

void OmahaService::OnPingReceived(const UpgradeRecommendedDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [[NSUserDefaults standardUserDefaults] setBool:details.is_up_to_date
                                          forKey:kIOSChromeUpToDateKey];

  // If there is a one-off callback, then it has the priority over the
  // scheduled ping.
  if (one_off_callback_) {
    std::move(one_off_callback_).Run(details);
    return;
  }

  if (upgrade_recommended_callback_) {
    if (!details.is_up_to_date) {
      upgrade_recommended_callback_.Run(details);
    }
  }
}
