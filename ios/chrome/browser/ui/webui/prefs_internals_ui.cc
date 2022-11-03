// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/prefs_internals_ui.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/url/chrome_url_constants.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/webui/url_data_source_ios.h"

namespace {

// A simple data source that returns the preferences for the associated browser
// state.
class PrefsInternalsSource : public web::URLDataSourceIOS {
 public:
  explicit PrefsInternalsSource(ChromeBrowserState* browser_state)
      : browser_state_(browser_state) {}

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
    // TODO(crbug.com/1006711): Properly disable this webui provider for
    // incognito browser states.
    if (browser_state_->IsOffTheRecord()) {
      std::move(callback).Run(nullptr);
      return;
    }

    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    std::string json;
    base::Value prefs = browser_state_->GetPrefs()->GetPreferenceValues(
        PrefService::INCLUDE_DEFAULTS);
    CHECK(base::JSONWriter::WriteWithOptions(
        prefs, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(json)));
  }

 private:
  ChromeBrowserState* browser_state_;
};

}  // namespace

PrefsInternalsUI::PrefsInternalsUI(web::WebUIIOS* web_ui,
                                   const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  web::URLDataSourceIOS::Add(browser_state,
                             new PrefsInternalsSource(browser_state));
}

PrefsInternalsUI::~PrefsInternalsUI() = default;
