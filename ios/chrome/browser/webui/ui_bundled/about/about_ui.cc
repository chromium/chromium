// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/about/about_ui.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kCreditsJsPath[] = "credits.js";
const char kCreditsCssPath[] = "credits.css";
const char kStringsJsPath[] = "strings.js";

class AboutUIHTMLSource : public web::URLDataSourceIOS {
 public:
  // Construct a data source for the specified `source_name`.
  explicit AboutUIHTMLSource(const std::string& source_name);

  AboutUIHTMLSource(const AboutUIHTMLSource&) = delete;
  AboutUIHTMLSource& operator=(const AboutUIHTMLSource&) = delete;

  // web::URLDataSourceIOS implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      std::string_view path,
      web::URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(std::string_view path) const override;
  bool ShouldDenyXFrameOptions() const override;

  // Send the response data.
  void FinishDataRequest(const std::string& html,
                         web::URLDataSourceIOS::GotDataCallback callback);

 private:
  ~AboutUIHTMLSource() override;

  std::string source_name_;
};

}  // namespace

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name)
    : source_name_(source_name) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() const {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    std::string_view path,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::string response;
  // Add your data source here, in alphabetical order.
  // keep-sorted start block=yes
  if (source_name_ == kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath) {
      idr = IDR_ABOUT_UI_CREDITS_JS;
    } else if (path == kCreditsCssPath) {
      idr = IDR_ABOUT_UI_CREDITS_CSS;
    }
    ui::ResourceBundle& resource_instance =
        ui::ResourceBundle::GetSharedInstance();
    response = resource_instance.LoadDataResourceString(idr);
  } else if (source_name_ == kChromeUIHistogramHost) {
    // Note: On other platforms, this is implemented in //content. If there is
    // ever a need for embedders other than //ios/chrome to use
    // chrome://histograms, this code could likely be moved to //ios/web.
    for (base::HistogramBase* histogram : base::StatisticsRecorder::Sort(
             base::StatisticsRecorder::GetHistograms())) {
      if (!base::Contains(histogram->histogram_name(), path)) {
        continue;
      }
      base::Value::Dict histogram_dict = histogram->ToGraphDict();
      std::string* header = histogram_dict.FindString("header");
      std::string* body = histogram_dict.FindString("body");

      response.append("<PRE>");
      response.append("<h4>");
      response.append(base::EscapeForHTML(*header));
      response.append("</h4>");
      response.append(base::EscapeForHTML(*body));
      response.append("</PRE>");
      response.append("<br><hr><br>");
    }
  }
  // keep-sorted end

  FinishDataRequest(response, std::move(callback));
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(html));
}

std::string AboutUIHTMLSource::GetMimeType(std::string_view path) const {
  if (path == kCreditsJsPath || path == kStringsJsPath) {
    return "application/javascript";
  }

  if (path == kCreditsCssPath) {
    return "text/css";
  }

  return "text/html";
}

bool AboutUIHTMLSource::ShouldDenyXFrameOptions() const {
  return web::URLDataSourceIOS::ShouldDenyXFrameOptions();
}

AboutUI::AboutUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::URLDataSourceIOS::Add(ProfileIOS::FromWebUIIOS(web_ui),
                             new AboutUIHTMLSource(host));
}

AboutUI::~AboutUI() {}
