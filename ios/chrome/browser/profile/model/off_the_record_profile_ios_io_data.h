// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IO_DATA_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IO_DATA_H_

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "ios/chrome/browser/net/model/net_types.h"
#import "ios/chrome/browser/profile/model/profile_ios_io_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class IOSChromeURLRequestContextGetter;

// OffTheRecordChromeBrowserState owns a
// OffTheRecordProfileIOSIOData::Handle, which holds a reference to the
// OffTheRecordProfileIOSIOData.
// OffTheRecordProfileIOSIOData is intended to own all the objects owned
// by OffTheRecordChromeBrowserState which live on the IO thread, such as, but
// not limited to, network objects like CookieMonster, HttpTransactionFactory,
// etc.
// OffTheRecordProfileIOSIOData is owned by the
// OffTheRecordChromeBrowserState and OffTheRecordProfileIOSIOData's
// IOSChromeURLRequestContexts. When all of them go away, then
// ProfileIOSIOData will be deleted. Note that the
// OffTheRecordProfileIOSIOData will typically outlive the profile
// it is "owned" by, so it's important for OffTheRecordProfileIOSIOData
// not to hold any references to the profile beyond what's used by
// LazyParams (which should be deleted after lazy initialization).
class OffTheRecordProfileIOSIOData : public ProfileIOSIOData {
 public:
  class Handle {
   public:
    explicit Handle(ProfileIOS* profile);

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    ~Handle();

    scoped_refptr<IOSChromeURLRequestContextGetter>
    CreateMainRequestContextGetter(ProtocolHandlerMap* protocol_handlers) const;

    // Clears the HTTP cache associated with the incognito profile.
    void DoomIncognitoCache();

    ProfileIOSIOData* io_data() const;

   private:
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
    const raw_ptr<OffTheRecordProfileIOSIOData> io_data_;

    const raw_ptr<ProfileIOS> profile_;

    mutable bool initialized_;
  };

  OffTheRecordProfileIOSIOData(const OffTheRecordProfileIOSIOData&) = delete;
  OffTheRecordProfileIOSIOData& operator=(const OffTheRecordProfileIOSIOData&) =
      delete;

 private:
  friend class base::RefCountedThreadSafe<OffTheRecordProfileIOSIOData>;

  OffTheRecordProfileIOSIOData();
  ~OffTheRecordProfileIOSIOData() override;

  void InitializeInternal(net::URLRequestContextBuilder* context_builder,
                          ProfileParams* profile_params) const override;

  // Server bound certificates and cookies are persisted to the disk on iOS.
  base::FilePath cookie_path_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IO_DATA_H_
