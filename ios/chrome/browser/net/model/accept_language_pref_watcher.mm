// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/accept_language_pref_watcher.h"

#import "base/check.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "net/http/http_util.h"

AcceptLanguagePrefWatcher::Handle::Handle(const std::string& languages) {
  SetAcceptLanguageHeaderFromPref(languages);
}

AcceptLanguagePrefWatcher::Handle::~Handle() = default;

void AcceptLanguagePrefWatcher::Handle::SetAcceptLanguageHeaderFromPref(
    const std::string& languages) {
  std::string header = net::HttpUtil::GenerateAcceptLanguageHeader(languages);
  {
    base::AutoLock locked(lock_);
    accept_language_header_ = header;
  }
}

std::string AcceptLanguagePrefWatcher::Handle::GetAcceptLanguageHeader() const {
  std::string result;
  {
    base::AutoLock locked(lock_);
    result = accept_language_header_;
  }
  return result;
}

AcceptLanguagePrefWatcher::AcceptLanguagePrefWatcher(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);

  handle_ = base::MakeRefCounted<Handle>(
      pref_service_->GetString(language::prefs::kAcceptLanguages));

  // Using base::Unretained(this) is safe as StringPrefMember is owned by the
  // current instance and will stop calling the callback once it is destroyed,
  // thus the callback won't outlive this object.
  accept_language_pref_.Init(
      language::prefs::kAcceptLanguages, pref_service_,
      base::BindRepeating(&AcceptLanguagePrefWatcher::OnPrefValueChanged,
                          base::Unretained(this)));
}

AcceptLanguagePrefWatcher::~AcceptLanguagePrefWatcher() {
  DCHECK(pref_service_);
  accept_language_pref_.Destroy();
  pref_service_ = nullptr;
  handle_.reset();
}

void AcceptLanguagePrefWatcher::OnPrefValueChanged(
    const std::string& pref_name) {
  DCHECK(pref_service_);
  DCHECK_EQ(pref_name, language::prefs::kAcceptLanguages);
  handle_->SetAcceptLanguageHeaderFromPref(
      pref_service_->GetString(language::prefs::kAcceptLanguages));
}

scoped_refptr<AcceptLanguagePrefWatcher::Handle>
AcceptLanguagePrefWatcher::GetHandle() {
  DCHECK(pref_service_);
  return handle_;
}
