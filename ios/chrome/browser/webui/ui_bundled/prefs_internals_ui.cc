// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/prefs_internals_ui.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "components/local_state/local_state_utils.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/webui/url_data_source_ios.h"

namespace {

// A simple data source that returns the preferences for the associated browser
// state.
class PrefsInternalsSource : public web::URLDataSourceIOS {
 public:
  explicit PrefsInternalsSource(ProfileIOS* profile) : profile_(profile) {}

  PrefsInternalsSource(const PrefsInternalsSource&) = delete;
  PrefsInternalsSource& operator=(const PrefsInternalsSource&) = delete;

  ~PrefsInternalsSource() override = default;

  // content::URLDataSource:
  std::string GetSource() const override { return kChromeUIPrefsInternalsHost; }

  std::string GetMimeType(const std::string& path) const override {
    return "text/plain";
  }

  void StartDataRequest(
      const std::string& path,
      web::URLDataSourceIOS::GotDataCallback callback) override {
    // TODO(crbug.com/40099982): Properly disable this webui provider for
    // incognito profiles.
    if (profile_->IsOffTheRecord()) {
      std::move(callback).Run(nullptr);
      return;
    }

    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        local_state_utils::GetPrefsAsJson(profile_->GetPrefs())
            .value_or(std::string())));
  }

 private:
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace

PrefsInternalsUI::PrefsInternalsUI(web::WebUIIOS* web_ui,
                                   const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::URLDataSourceIOS::Add(profile, new PrefsInternalsSource(profile));
}

PrefsInternalsUI::~PrefsInternalsUI() = default;
