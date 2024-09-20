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
#include "url/gurl.h"

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
      const std::string& path,
      web::URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldDenyXFrameOptions() const override;

  // Send the response data.
  void FinishDataRequest(const std::string& html,
                         web::URLDataSourceIOS::GotDataCallback callback);

 private:
  ~AboutUIHTMLSource() override;

  std::string source_name_;
};

void AppendHeader(std::string* output,
                  int refresh,
                  const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(base::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
  output->append("<meta charset='utf-8'>\n");
  if (refresh > 0) {
    output->append("<meta http-equiv='refresh' content='");
    output->append(base::NumberToString(refresh));
    output->append("'/>\n");
  }
}

void AppendBody(std::string* output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string* output) {
  output->append("</body>\n</html>\n");
}

std::string ChromeURLs() {
  std::string html;
  AppendHeader(&html, 0, "Chrome URLs");
  AppendBody(&html);
  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  std::vector<std::string> hosts(kChromeHostURLs,
                                 kChromeHostURLs + kNumberOfChromeHostURLs);
  std::sort(hosts.begin(), hosts.end());
  for (std::vector<std::string>::const_iterator i = hosts.begin();
       i != hosts.end(); ++i)
    html += "<li><a href='chrome://" + *i + "/' id='" + *i + "'>chrome://" +
            *i + "</a></li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

}  // namespace

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name)
    : source_name_(source_name) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() const {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    const std::string& path,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::string response;
  // Add your data source here, in alphabetical order.
  if (source_name_ == kChromeUIChromeURLsHost) {
    response = ChromeURLs();
  } else if (source_name_ == kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath)
      idr = IDR_ABOUT_UI_CREDITS_JS;
    else if (path == kCreditsCssPath)
      idr = IDR_ABOUT_UI_CREDITS_CSS;
    ui::ResourceBundle& resource_instance =
        ui::ResourceBundle::GetSharedInstance();
    response = resource_instance.LoadDataResourceString(idr);
  } else if (source_name_ == kChromeUIHistogramHost) {
    // Note: On other platforms, this is implemented in //content. If there is
    // ever a need for embedders other than //ios/chrome to use
    // chrome://histograms, this code could likely be moved to //ios/web.
    for (base::HistogramBase* histogram : base::StatisticsRecorder::Sort(
             base::StatisticsRecorder::GetHistograms())) {
      std::string histogram_name = histogram->histogram_name();
      if (!base::Contains(histogram_name, path)) {
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

  FinishDataRequest(response, std::move(callback));
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(html));
}

std::string AboutUIHTMLSource::GetMimeType(const std::string& path) const {
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
