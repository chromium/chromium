// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_ACCEPT_LANGUAGE_PREF_WATCHER_H_
#define IOS_CHROME_BROWSER_NET_MODEL_ACCEPT_LANGUAGE_PREF_WATCHER_H_

#include <string>

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/prefs/pref_member.h"

// Helper class to watch the value of AcceptLanguage preference on the UI
// thread and safely propagate the value to the IO thread.
class AcceptLanguagePrefWatcher {
 public:
  // The Handle class is a thread safe ref-counted object that holds the
  // most recent version of the value of the AcceptLanguage in a format
  // usable as an HTTP header.
  class Handle : public base::RefCountedThreadSafe<Handle> {
   public:
    // Initializes the AcceptLanguage header from the preference value.
    Handle(const std::string& languages);

    Handle(const Handle&) = delete;
    const Handle& operator=(const Handle&) = delete;

    // Sets the value for the AcceptLanguage header from the pref value.
    void SetAcceptLanguageHeaderFromPref(const std::string& languages);

    // Gets the value for the AcceptLanguage header.
    std::string GetAcceptLanguageHeader() const;

   private:
    friend class base::RefCountedThreadSafe<Handle>;
    ~Handle();

    mutable base::Lock lock_;
    std::string accept_language_header_ GUARDED_BY(lock_);
  };

  // Initializes the instance with `pref_service` which must be valid and
  // whose lifetime must exceed that of the AcceptLanguagePrefWatcher
  explicit AcceptLanguagePrefWatcher(PrefService* pref_service);

  AcceptLanguagePrefWatcher(const AcceptLanguagePrefWatcher&) = delete;
  const AcceptLanguagePrefWatcher& operator=(const AcceptLanguagePrefWatcher&) =
      delete;

  ~AcceptLanguagePrefWatcher();

  // Returns a pointer to the handle. This can be used on any thread to read
  // the last value of the AcceptLanguage preference in a format usable as an
  // HTTP header.
  scoped_refptr<Handle> GetHandle();

 private:
  // Invoked by StringPrefMember when the preference value changes.
  void OnPrefValueChanged(const std::string& pref_name);

  raw_ptr<PrefService> pref_service_ = nullptr;
  StringPrefMember accept_language_pref_;
  scoped_refptr<Handle> handle_;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_ACCEPT_LANGUAGE_PREF_WATCHER_H_
