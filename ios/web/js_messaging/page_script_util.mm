// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}

NSString* MakeScriptInjectableOnce(NSString* script_identifier,
                                   NSString* script) {
  NSString* kOnceWrapperTemplate =
      @"if (typeof %@ === 'undefined') { var %@ = true; %%@ }";
  NSString* injected_var_name =
      [NSString stringWithFormat:@"_injected_%@", script_identifier];
  NSString* once_wrapper =
      [NSString stringWithFormat:kOnceWrapperTemplate, injected_var_name,
                                 injected_var_name];
  return [NSString stringWithFormat:once_wrapper, script];
}

NSString* GetDocumentStartScriptForMainFrame(BrowserState* browser_state) {
  DCHECK(GetWebClient());
  NSString* embedder_page_script =
      GetWebClient()->GetDocumentStartScriptForMainFrame(browser_state);
  DCHECK(embedder_page_script);

  NSString* web_bundle = GetPageScript(@"main_frame_web_bundle");
  DCHECK(web_bundle);

  NSString* script =
      [NSString stringWithFormat:@"%@; %@", web_bundle, embedder_page_script];
  return MakeScriptInjectableOnce(@"start_main_frame", script);
}

NSString* GetDocumentStartScriptForAllFrames(BrowserState* browser_state) {
  DCHECK(GetWebClient());
  NSString* embedder_page_script =
      GetWebClient()->GetDocumentStartScriptForAllFrames(browser_state);
  DCHECK(embedder_page_script);
  NSString* web_bundle = GetPageScript(@"all_frames_web_bundle");
  NSString* injectedCookieState = @"allow";
  switch (browser_state->GetCookieBlockingMode()) {
    case CookieBlockingMode::kBlock:
      injectedCookieState = @"block";
      break;
    case CookieBlockingMode::kBlockThirdParty:
      injectedCookieState = @"block-third-party";
      break;
    case CookieBlockingMode::kAllow:
      injectedCookieState = @"allow";
      break;
  }
  web_bundle =
      [web_bundle stringByReplacingOccurrencesOfString:@"$(COOKIE_STATE)"
                                            withString:injectedCookieState];
  NSString* script =
      [NSString stringWithFormat:@"%@; %@", web_bundle, embedder_page_script];
  return MakeScriptInjectableOnce(@"start_all_frames", script);
}

NSString* GetDocumentEndScriptForAllFrames(BrowserState* browser_state) {
  return MakeScriptInjectableOnce(
      @"end_all_frames", GetPageScript(@"all_frames_document_end_web_bundle"));
}

}  // namespace web
