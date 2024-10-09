// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_H_

#include <map>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/net/model/net_types.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#include "ios/web/public/browser_state.h"
#include "net/url_request/url_request_job_factory.h"

class BrowserStatePolicyConnector;
class PrefProxyConfigTracker;
class PrefService;
class ProfileIOSIOData;

namespace base {
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace web {
class WebUIIOS;
}

namespace policy {
class UserCloudPolicyManager;
}

enum class ProfileIOSType {
  REGULAR_PROFILE,
  INCOGNITO_PROFILE,
};

// This class is a Chrome-specific extension of the BrowserState interface.
class ProfileIOS : public web::BrowserState {
 public:
  enum class CreationMode {
    kSynchronous,
    kAsynchronous,
  };

  // Delegate notified of ProfileIOS creation events.
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Called when creation of the Profile is started.
    virtual void OnProfileCreationStarted(ProfileIOS* profile,
                                          CreationMode creation_mode) = 0;

    // Called when creation of the Profile is finished.
    virtual void OnProfileCreationFinished(ProfileIOS* profile,
                                           CreationMode creation_mode,
                                           bool is_new_profile,
                                           bool success) = 0;
  };

  ProfileIOS(const ProfileIOS&) = delete;
  ProfileIOS& operator=(const ProfileIOS&) = delete;

  ~ProfileIOS() override;

  // Creates a new Profile at `path` with `creation_mode`. If not null,
  // `delegate` will be notified when the creation starts and completes.
  static std::unique_ptr<ProfileIOS> CreateProfile(
      const base::FilePath& path,
      std::string_view profile_name,
      CreationMode creation_mode,
      Delegate* delegate);

  // Returns the ProfileIOS corresponding to the given BrowserState.
  static ProfileIOS* FromBrowserState(BrowserState* profile);

  // Returns the ProfileIOS corresponding to the given WebUIIOS.
  static ProfileIOS* FromWebUIIOS(web::WebUIIOS* web_ui);

  // Returns sequenced task runner where profile dependent I/O
  // operations should be performed.
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner();

  // Returns the original "recording" ProfileIOS. This method returns
  // `this` if the ProfileIOS is not incognito.
  // TODO(crbug.com/358299863): Remove this function once fully migrated.
  virtual ProfileIOS* GetOriginalChromeBrowserState() = 0;

  // Returns the original "recording" Profile. This method returns `this` if the
  // Profile is not incognito.
  virtual ProfileIOS* GetOriginalProfile() = 0;

  // Returns true if the ProfileIOS is off-the-record or if the
  // associated off-the-record profile has been created.
  // Calling this method does not create the off-the-record profile if it
  // does not already exist.
  // TODO(crbug.com/358299863): Remove this function once fully migrated.
  virtual bool HasOffTheRecordChromeBrowserState() const = 0;

  // Returns true if the Profile is off-the-record or if the associated
  // off-the-record profile has been created. Calling this method does not
  // create the off-the-record profile if it does not already exist.
  virtual bool HasOffTheRecordProfile() const = 0;

  // Returns the incognito version of this ProfileIOS. The returned
  // ProfileIOS instance is owned by this ProfileIOS instance.
  // WARNING: This will create the OffTheRecord ProfileIOS if it
  // doesn't already exist.
  // TODO(crbug.com/358299863): Remove this function once fully migrated.
  virtual ProfileIOS* GetOffTheRecordChromeBrowserState() = 0;

  // Returns the incognito version of this Profile. The returned Profile
  // instance is owned by this Profile instance. WARNING: This will create the
  // OffTheRecord Profile if it doesn't already exist.
  virtual ProfileIOS* GetOffTheRecordProfile() = 0;

  // Destroys the OffTheRecord ProfileIOS that is associated with this
  // ProfileIOS, if one exists.
  // TODO(crbug.com/358299863): Remove this function once fully migrated.
  virtual void DestroyOffTheRecordChromeBrowserState() = 0;

  // Destroys the OffTheRecord Profile that is associated with this Profile, if
  // one exists.
  virtual void DestroyOffTheRecordProfile() = 0;

  // Retrieves a pointer to the BrowserStatePolicyConnector that manages policy
  // for this BrowserState. May return nullptr if policy is disabled.
  virtual BrowserStatePolicyConnector* GetPolicyConnector() = 0;

  // Returns a pointer to the UserCloudPolicyManager that is a facade for the
  // user cloud policy system.
  virtual policy::UserCloudPolicyManager* GetUserCloudPolicyManager() = 0;

  // Retrieves a pointer to the PrefService that manages the preferences.
  virtual PrefService* GetPrefs();
  virtual const PrefService* GetPrefs() const;

  // Retrieves a pointer to the PrefService that manages the preferences as
  // a sync_preferences::PrefServiceSyncable.
  virtual sync_preferences::PrefServiceSyncable* GetSyncablePrefs() = 0;
  virtual const sync_preferences::PrefServiceSyncable* GetSyncablePrefs()
      const = 0;

  // Allows access to ProfileIOSIOData without going through
  // ResourceContext that is not compiled on iOS. This method must be called on
  // UI thread, but the returned object must only be accessed on the IO thread.
  virtual ProfileIOSIOData* GetIOData() = 0;

  // Deletes all network related data since `time`. It deletes transport
  // security state since `time` and it also deletes HttpServerProperties data.
  // Works asynchronously, however if the `completion` callback is non-null, it
  // will be posted on the UI thread once the removal process completes.
  // Be aware that theoretically it is possible that `completion` will be
  // invoked after the BrowserState instance has been destroyed.
  virtual void ClearNetworkingHistorySince(base::Time time,
                                           base::OnceClosure completion) = 0;

  // Returns the Profile name. This is empty for off-the-record Profiles.
  const std::string& GetProfileName() const;

  // Returns the helper object that provides the proxy configuration service
  // access to the the proxy configuration possibly defined by preferences.
  virtual PrefProxyConfigTracker* GetProxyConfigTracker() = 0;

  // Creates the main net::URLRequestContextGetter that will be returned by
  // GetRequestContext(). Should only be called once.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) = 0;

  // Returns a weak pointer to the current instance.
  virtual base::WeakPtr<ProfileIOS> AsWeakPtr() = 0;

  // Returns the path where the off-the-record BrowserState data is stored.
  // If the BrowserState is off-the-record, this is equal to GetStatePath().
  base::FilePath GetOffTheRecordStatePath() const;

  // web::BrowserState
  base::FilePath GetStatePath() const final;
  net::URLRequestContextGetter* GetRequestContext() final;
  void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params) final;

 protected:
  explicit ProfileIOS(const base::FilePath& state_path,
                      std::string_view profile_name,
                      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

 private:
  base::FilePath const state_path_;
  std::string const profile_name_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

using ChromeBrowserState = ProfileIOS;

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_IOS_H_
