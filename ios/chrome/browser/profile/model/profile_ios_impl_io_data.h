// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_IO_DATA_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_IO_DATA_H_

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "components/prefs/pref_store.h"
#import "ios/chrome/browser/net/model/net_types.h"
#import "ios/chrome/browser/profile/model/profile_ios_io_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class JsonPrefStore;

namespace net {
class CookieStore;
class HttpNetworkSession;
class HttpTransactionFactory;
class URLRequestJobFactory;
}  // namespace net

class ProfileIOSImplIOData : public ProfileIOSIOData {
 public:
  class Handle {
   public:
    explicit Handle(ProfileIOS* profile);

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    ~Handle();

    // Init() must be called before ~Handle(). It records most of the
    // parameters needed to construct a ChromeURLRequestContextGetter.
    void Init(const base::FilePath& cookie_path,
              const base::FilePath& cache_path,
              int cache_max_size,
              const base::FilePath& profile_path);

    // These Create*ContextGetter() functions are only exposed because the
    // circular relationship between ChromeBrowserState,
    // ProfileIOSIOData::Handle, and the
    // IOSChromeURLRequestContextGetter factories requires ChromeBrowserState be
    // able to call these functions.
    scoped_refptr<IOSChromeURLRequestContextGetter>
    CreateMainRequestContextGetter(ProtocolHandlerMap* protocol_handlers,
                                   PrefService* local_state,
                                   IOSChromeIOThread* io_thread) const;

    ProfileIOSIOData* io_data() const;

    // Deletes all network related data since `time`. It deletes transport
    // security state since `time` and also deletes HttpServerProperties data.
    // Works asynchronously, however if the `completion` callback is non-null,
    // it will be posted on the UI thread once the removal process completes.
    void ClearNetworkingHistorySince(base::Time time,
                                     base::OnceClosure completion);

   private:
    typedef std::map<base::FilePath,
                     scoped_refptr<IOSChromeURLRequestContextGetter>>
        IOSChromeURLRequestContextGetterMap;

    // Lazily initialize ProfileParams. We do this on the calls to
    // Get*RequestContextGetter(), so we only initialize ProfileParams right
    // before posting a task to the IO thread to start using them. This prevents
    // objects that are supposed to be deleted on the IO thread, but are created
    // on the UI thread from being unnecessarily initialized.
    void LazyInitialize() const;

    // Collect references to context getters in reverse order, i.e. last item
    // will be main request getter. This list is passed to `io_data_`
    // for invalidation on IO thread.
    std::unique_ptr<IOSChromeURLRequestContextGetterVector>
    GetAllContextGetters();

    // The getters will be invalidated on the IO thread before
    // ProfileIOSIOData instance is deleted.
    mutable scoped_refptr<IOSChromeURLRequestContextGetter>
        main_request_context_getter_;
    mutable IOSChromeURLRequestContextGetterMap app_request_context_getter_map_;
    const raw_ptr<ProfileIOSImplIOData> io_data_;

    const raw_ptr<ProfileIOS> profile_;

    mutable bool initialized_;
  };

  ProfileIOSImplIOData(const ProfileIOSImplIOData&) = delete;
  ProfileIOSImplIOData& operator=(const ProfileIOSImplIOData&) = delete;

 private:
  friend class base::RefCountedThreadSafe<ProfileIOSImplIOData>;

  struct LazyParams {
    LazyParams();
    ~LazyParams();

    // All of these parameters are intended to be read on the IO thread.
    base::FilePath cookie_path;
    base::FilePath cache_path;
    int cache_max_size;
  };

  ProfileIOSImplIOData();
  ~ProfileIOSImplIOData() override;

  void InitializeInternal(net::URLRequestContextBuilder* context_builder,
                          ProfileParams* profile_params) const override;

  // Deletes all network related data since `time`. It deletes transport
  // security state since `time` and also deletes HttpServerProperties data.
  // Works asynchronously, however if the `completion` callback is non-null,
  // it will be posted on the UI thread once the removal process completes.
  void ClearNetworkingHistorySinceOnIOThread(base::Time time,
                                             base::OnceClosure completion);

  // Lazy initialization params.
  mutable std::unique_ptr<LazyParams> lazy_params_;

  mutable scoped_refptr<JsonPrefStore> network_json_store_;

  mutable std::unique_ptr<net::HttpNetworkSession> http_network_session_;
  mutable std::unique_ptr<net::HttpTransactionFactory> main_http_factory_;

  mutable std::unique_ptr<net::CookieStore> main_cookie_store_;

  mutable std::unique_ptr<net::URLRequestJobFactory> main_job_factory_;

  // Parameters needed for isolated apps.
  base::FilePath profile_path_;
  int app_cache_max_size_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_IO_DATA_H_
