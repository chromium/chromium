// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_IO_DATA_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_IO_DATA_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_store.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_io_data.h"
#include "ios/chrome/browser/net/net_types.h"

class ChromeBrowserState;
class JsonPrefStore;

namespace net {
class CookieStore;
class HttpNetworkSession;
class HttpTransactionFactory;
class URLRequestJobFactory;
}  // namespace net

class ChromeBrowserStateImplIOData : public ChromeBrowserStateIOData {
 public:
  class Handle {
   public:
    explicit Handle(ChromeBrowserState* browser_state);

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
    // ChromeBrowserStateIOData::Handle, and the
    // IOSChromeURLRequestContextGetter factories requires ChromeBrowserState be
    // able to call these functions.
    scoped_refptr<IOSChromeURLRequestContextGetter>
    CreateMainRequestContextGetter(ProtocolHandlerMap* protocol_handlers,
                                   PrefService* local_state,
                                   IOSChromeIOThread* io_thread) const;

    ChromeBrowserStateIOData* io_data() const;

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
    // ChromeBrowserStateIOData instance is deleted.
    mutable scoped_refptr<IOSChromeURLRequestContextGetter>
        main_request_context_getter_;
    mutable IOSChromeURLRequestContextGetterMap app_request_context_getter_map_;
    ChromeBrowserStateImplIOData* const io_data_;

    ChromeBrowserState* const browser_state_;

    mutable bool initialized_;
  };

  ChromeBrowserStateImplIOData(const ChromeBrowserStateImplIOData&) = delete;
  ChromeBrowserStateImplIOData& operator=(const ChromeBrowserStateImplIOData&) =
      delete;

 private:
  friend class base::RefCountedThreadSafe<ChromeBrowserStateImplIOData>;

  struct LazyParams {
    LazyParams();
    ~LazyParams();

    // All of these parameters are intended to be read on the IO thread.
    base::FilePath cookie_path;
    base::FilePath cache_path;
    int cache_max_size;
  };

  ChromeBrowserStateImplIOData();
  ~ChromeBrowserStateImplIOData() override;

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

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_IO_DATA_H_
