// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/terms_ui.h"

#import <Foundation/Foundation.h>

#import "base/apple/bundle_locations.h"
#import "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/terms_util.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"

namespace {

class TermsUIHTMLSource : public web::URLDataSourceIOS {
 public:
  // Construct a data source for the specified `source_name`.
  explicit TermsUIHTMLSource(const std::string& source_name);

  TermsUIHTMLSource(const TermsUIHTMLSource&) = delete;
  TermsUIHTMLSource& operator=(const TermsUIHTMLSource&) = delete;

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
  ~TermsUIHTMLSource() override;

  std::string source_name_;
};

}  // namespace

TermsUIHTMLSource::TermsUIHTMLSource(const std::string& source_name)
    : source_name_(source_name) {}

TermsUIHTMLSource::~TermsUIHTMLSource() {}

std::string TermsUIHTMLSource::GetSource() const {
  return source_name_;
}

void TermsUIHTMLSource::StartDataRequest(
    const std::string& path,
    web::URLDataSourceIOS::GotDataCallback callback) {
  NSString* terms_of_service_path =
      base::SysUTF8ToNSString(GetTermsOfServicePath());
  NSString* bundle_path = [base::apple::FrameworkBundle() bundlePath];
  NSString* full_path =
      [bundle_path stringByAppendingPathComponent:terms_of_service_path];
  DCHECK(full_path);

  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:full_path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error && [content length]);
  FinishDataRequest(base::SysNSStringToUTF8(content), std::move(callback));
}

void TermsUIHTMLSource::FinishDataRequest(
    const std::string& html,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(html));
}

std::string TermsUIHTMLSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

bool TermsUIHTMLSource::ShouldDenyXFrameOptions() const {
  return web::URLDataSourceIOS::ShouldDenyXFrameOptions();
}

TermsUI::TermsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::URLDataSourceIOS::Add(ProfileIOS::FromWebUIIOS(web_ui),
                             new TermsUIHTMLSource(host));
}

TermsUI::~TermsUI() {}
