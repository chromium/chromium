// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/terms_ui.h"

#import <Foundation/Foundation.h>

#include "base/mac/bundle_locations.h"
#include "base/memory/ref_counted_memory.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/util/terms_util.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/public/webui/web_ui_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TermsUIHTMLSource : public web::URLDataSourceIOS {
 public:
  // Construct a data source for the specified |source_name|.
  explicit TermsUIHTMLSource(const std::string& source_name);

  // web::URLDataSourceIOS implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const web::URLDataSourceIOS::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldDenyXFrameOptions() const override;

  // Send the response data.
  void FinishDataRequest(
      const std::string& html,
      const web::URLDataSourceIOS::GotDataCallback& callback);

 private:
  ~TermsUIHTMLSource() override;

  std::string source_name_;

  DISALLOW_COPY_AND_ASSIGN(TermsUIHTMLSource);
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
    const web::URLDataSourceIOS::GotDataCallback& callback) {
  NSString* terms_of_service_path =
      base::SysUTF8ToNSString(GetTermsOfServicePath());
  NSString* bundle_path = [base::mac::FrameworkBundle() bundlePath];
  NSString* full_path =
      [bundle_path stringByAppendingPathComponent:terms_of_service_path];
  DCHECK(full_path);

  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:full_path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error && [content length]);
  FinishDataRequest(base::SysNSStringToUTF8(content), callback);
}

void TermsUIHTMLSource::FinishDataRequest(
    const std::string& html,
    const web::URLDataSourceIOS::GotDataCallback& callback) {
  std::string html_copy(html);
  callback.Run(base::RefCountedString::TakeString(&html_copy));
}

std::string TermsUIHTMLSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

bool TermsUIHTMLSource::ShouldDenyXFrameOptions() const {
  return web::URLDataSourceIOS::ShouldDenyXFrameOptions();
}

TermsUI::TermsUI(web::WebUIIOS* web_ui, const std::string& name)
    : web::WebUIIOSController(web_ui) {
  web::URLDataSourceIOS::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                             new TermsUIHTMLSource(name));
}

TermsUI::~TermsUI() {}
