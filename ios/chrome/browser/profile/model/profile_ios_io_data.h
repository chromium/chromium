// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IO_DATA_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IO_DATA_H_

#import <map>
#import <memory>
#import <string>
#import <vector>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/prefs/pref_member.h"
#import "ios/chrome/browser/net/model/net_types.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "net/cookies/cookie_monster.h"
#import "net/http/http_cache.h"
#import "net/http/http_network_session.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_interceptor.h"
#import "net/url_request/url_request_job_factory.h"

class AcceptLanguagePrefWatcher;
enum class ProfileIOSType;
class HostContentSettingsMap;
class IOSChromeHttpUserAgentSettings;
class IOSChromeURLRequestContextGetter;

namespace content_settings {
class CookieSettings;
}

namespace net {
class HttpTransactionFactory;
class ProxyConfigService;
class SystemCookieStore;
class URLRequestContextBuilder;
}  // namespace net

// Conceptually speaking, the ProfileIOSIOData represents data that
// lives on the IO thread that is owned by a ProfileIOS, such as, but
// not limited to, network objects like CookieMonster, HttpTransactionFactory,
// etc.
// ProfileIOS owns ProfileIOSIOData, but will make sure to
// delete it on the IO thread.
class ProfileIOSIOData {
 public:
  typedef std::vector<scoped_refptr<IOSChromeURLRequestContextGetter>>
      IOSChromeURLRequestContextGetterVector;

  ProfileIOSIOData(const ProfileIOSIOData&) = delete;
  ProfileIOSIOData& operator=(const ProfileIOSIOData&) = delete;

  virtual ~ProfileIOSIOData();

  // Initializes the ProfileIOSIOData object and primes the
  // RequestContext generation. Must be called prior to any of the Get*()
  // methods other than GetResouceContext or GetMetricsEnabledStateOnIOThread.
  void Init(ProtocolHandlerMap* protocol_handlers) const;

  net::URLRequestContext* GetMainRequestContext() const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  content_settings::CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  ProfileIOSType profile_type() const { return profile_type_; }

  bool IsOffTheRecord() const;

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this profile resides. This is safe for use from the IO
  // thread, and should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

 protected:
  // Created on the UI thread, read on the IO thread during
  // ProfileIOSIOData lazy initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    base::FilePath path;
    raw_ptr<IOSChromeIOThread> io_thread;
    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;

    // We need to initialize the ProxyConfigService from the UI thread
    // because on linux it relies on initializing things through gsettings,
    // and needs to be on the main thread.
    std::unique_ptr<net::ProxyConfigService> proxy_config_service;

    // SystemCookieStore should be initialized from the UI thread as it depends
    // on the `profile`.
    std::unique_ptr<net::SystemCookieStore> system_cookie_store;

    // The profile this struct was populated from. It's passed as a void*
    // to ensure it's not accidentally used on the IO thread.
    raw_ptr<void> profile;
  };

  explicit ProfileIOSIOData(ProfileIOSType profile_type);

  void InitializeOnUIThread(ProfileIOS* profile);

  // Called when the ChromeBrowserState is destroyed. `context_getters` must
  // include all URLRequestContextGetters that refer to the
  // ProfileIOSIOData's URLRequestContexts. Triggers destruction of the
  // ProfileIOSIOData and shuts down `context_getters` safely on the IO
  // thread.
  void ShutdownOnUIThread(
      std::unique_ptr<IOSChromeURLRequestContextGetterVector> context_getters);

  net::URLRequestContext* main_request_context() const {
    return main_request_context_.get();
  }

  bool initialized() const { return initialized_; }

 private:
  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does the actual initialization of the ProfileIOSIOData subtype.
  // Subtypes should use the static helper functions above to implement this.
  virtual void InitializeInternal(
      net::URLRequestContextBuilder* context_builder,
      ProfileParams* profile_params) const = 0;

  // The order *DOES* matter for the majority of these member variables, so
  // don't move them around unless you know what you're doing!
  // General rules:
  //   * ResourceContext references the URLRequestContexts, so
  //   URLRequestContexts must outlive ResourceContext, hence ResourceContext
  //   should be destroyed first.
  //   * URLRequestContexts reference a whole bunch of members, so
  //   URLRequestContext needs to be destroyed before them.
  //   * Therefore, ResourceContext should be listed last, and then the
  //   URLRequestContexts, and then the URLRequestContext members.
  //   * Note that URLRequestContext members have a directed dependency graph
  //   too, so they must themselves be ordered correctly.

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the ChromeBrowserState, used to initialize
  // ProfileIOSIOData. Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember enable_referrers_;
  mutable BooleanPrefMember enable_do_not_track_;

  BooleanPrefMember enable_metrics_;
  std::unique_ptr<AcceptLanguagePrefWatcher> accept_language_pref_watcher_;

  // These are only valid in between LazyInitialize() and their accessor being
  // called.
  mutable std::unique_ptr<net::URLRequestContext> main_request_context_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  mutable std::unique_ptr<IOSChromeHttpUserAgentSettings>
      chrome_http_user_agent_settings_;

  const ProfileIOSType profile_type_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IO_DATA_H_
