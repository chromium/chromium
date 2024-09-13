// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/userdefaults_internals_ui.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"

namespace {

// A simple data source that returns the user defaults preferences.
class UserDefaultsInternalsSource : public web::URLDataSourceIOS {
 public:
  explicit UserDefaultsInternalsSource(bool is_incognito)
      : is_incognito_(is_incognito) {}

  UserDefaultsInternalsSource(const UserDefaultsInternalsSource&) = delete;
  UserDefaultsInternalsSource& operator=(const UserDefaultsInternalsSource&) =
      delete;

  ~UserDefaultsInternalsSource() override = default;

  // web::URLDataSourceIOS
  std::string GetSource() const override {
    return kChromeUIUserDefaultsInternalsHost;
  }

  std::string GetMimeType(const std::string& path) const override {
    return "text/plain";
  }

  void StartDataRequest(
      const std::string& path,
      web::URLDataSourceIOS::GotDataCallback callback) override {
    std::string response;

    if (is_incognito_) {
      response.append("This page is not available in incognito mode.");
      FinishDataRequest(std::move(response), std::move(callback));
      return;
    }
    response.append("<!DOCTYPE HTML>\n<html>\n<head>\n");
    response.append(
        "<style>table, th, td {border: 1px solid black; "
        "border-collapse: collapse;}th {text-align: left;}</style>");
    response.append("<h1>List of user defaults:</h1>\n\n");

    NSDictionary<NSString*, id>* defaultsDict =
        [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];
    NSArray* sortedKeys = [[defaultsDict allKeys]
        sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];

    response.append("<table>\n");
    for (NSString* key in sortedKeys) {
      id value = defaultsDict[key];
      NSString* valueString;
      if ([value isKindOfClass:[NSArray class]]) {
        valueString = [value description];
      } else if ([value isKindOfClass:[NSDictionary class]]) {
        valueString = [value description];
      } else if ([value isKindOfClass:[NSData class]]) {
        valueString = [value description];
      } else if ([value isKindOfClass:[NSString class]]) {
        valueString = base::apple::ObjCCastStrict<NSString>(value);
      } else if ([value isKindOfClass:[NSNumber class]]) {
        valueString =
            [base::apple::ObjCCastStrict<NSNumber>(value) stringValue];
      } else if ([value isKindOfClass:[NSDate class]]) {
        valueString = [value description];
      }
      response.append("<tr>\n");
      response.append("<th>" + base::SysNSStringToUTF8(key) + "</th>\n");
      response.append("<td>" + base::SysNSStringToUTF8(valueString) +
                      "</td>\n");
      response.append("</tr>\n");
    }
    response.append("</table>\n<html>");
    FinishDataRequest(std::move(response), std::move(callback));
  }

  void FinishDataRequest(std::string html,
                         web::URLDataSourceIOS::GotDataCallback callback) {
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(html)));
  }

 private:
  bool is_incognito_;
};

}  // anonymous namespace

UserDefaultsInternalsUI::UserDefaultsInternalsUI(web::WebUIIOS* web_ui,
                                                 const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::URLDataSourceIOS::Add(
      profile, new UserDefaultsInternalsSource(profile->IsOffTheRecord()));
}

UserDefaultsInternalsUI::~UserDefaultsInternalsUI() = default;
