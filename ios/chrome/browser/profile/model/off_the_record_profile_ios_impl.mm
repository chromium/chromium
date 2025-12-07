// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/proxy_config/ios/proxy_service_factory.h"
#import "components/proxy_config/pref_proxy_config_tracker.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/user_prefs/user_prefs.h"
#import "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"
#import "ios/chrome/browser/profile/model/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_dependency_manager_ios.h"

OffTheRecordProfileIOSImpl::OffTheRecordProfileIOSImpl(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    ProfileIOS* original_profile,
    const base::FilePath& otr_path)
    : ProfileIOS(otr_path,
                 /*profile_name=*/std::string(),
                 std::move(io_task_runner)),
      original_profile_(original_profile),
      start_time_(base::Time::Now()),
      prefs_(CreateIncognitoProfilePrefs(
          static_cast<sync_preferences::PrefServiceSyncable*>(
              original_profile_->GetPrefs()))) {
  ProfileDependencyManagerIOS::GetInstance()->MarkProfileLive(this);

  user_prefs::UserPrefs::Set(this, GetPrefs());
  io_data_.reset(new OffTheRecordProfileIOSIOData::Handle(this));
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kIncognito);
  base::RecordAction(base::UserMetricsAction("IncognitoMode_Started"));

  // DO NOT ADD ANY INITIALISATION AFTER THIS LINE.

  // The initialisation of the ProfileIOS is now complete and the
  // service can be safely created.
  ProfileDependencyManagerIOS::GetInstance()->CreateProfileServices(this);
}

OffTheRecordProfileIOSImpl::~OffTheRecordProfileIOSImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Notify the callback of the profile destruction before destroying anything.
  NotifyProfileDestroyed();

  ProfileDependencyManagerIOS::GetInstance()->DestroyProfileServices(this);
  if (pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_->DetachFromPrefService();
  }

  const base::TimeDelta duration = base::Time::Now() - start_time_;
  base::UmaHistogramCustomCounts("Profile.Incognito.Lifetime",
                                 duration.InMinutes(), 1,
                                 base::Days(28).InMinutes(), 100);

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  GetApplicationContext()->GetIOSChromeIOThread()->ChangedToOnTheRecord();
}

ProfileIOS* OffTheRecordProfileIOSImpl::GetOriginalProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_profile_;
}

bool OffTheRecordProfileIOSImpl::HasOffTheRecordProfile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

ProfileIOS* OffTheRecordProfileIOSImpl::GetOffTheRecordProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

void OffTheRecordProfileIOSImpl::DestroyOffTheRecordProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

ProfilePolicyConnector* OffTheRecordProfileIOSImpl::GetPolicyConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Forward the call to the original (non-OTR) profile.
  return GetOriginalProfile()->GetPolicyConnector();
}

policy::UserCloudPolicyManager*
OffTheRecordProfileIOSImpl::GetUserCloudPolicyManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Forward the call to the original (non-OTR) profile.
  return GetOriginalProfile()->GetUserCloudPolicyManager();
}

sync_preferences::PrefServiceSyncable*
OffTheRecordProfileIOSImpl::GetSyncablePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prefs_.get();
}

const sync_preferences::PrefServiceSyncable*
OffTheRecordProfileIOSImpl::GetSyncablePrefs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prefs_.get();
}

bool OffTheRecordProfileIOSImpl::IsOffTheRecord() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

PrefProxyConfigTracker* OffTheRecordProfileIOSImpl::GetProxyConfigTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}

ProfileIOSIOData* OffTheRecordProfileIOSImpl::GetIOData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return io_data_->io_data();
}

net::URLRequestContextGetter* OffTheRecordProfileIOSImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return io_data_->CreateMainRequestContextGetter(protocol_handlers).get();
}

base::WeakPtr<ProfileIOS> OffTheRecordProfileIOSImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void OffTheRecordProfileIOSImpl::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do here, our transport security state is read-only.
  // Still, fire the callback to indicate we have finished, otherwise the
  // BrowsingDataRemover will never be destroyed and the dialog will never be
  // closed. We must do this asynchronously in order to avoid reentrancy issues.
  if (!completion.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(completion));
  }
}
