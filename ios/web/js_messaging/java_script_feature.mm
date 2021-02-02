// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/js_messaging/java_script_feature.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/java_script_content_world.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#include "ios/web/js_messaging/page_script_util.h"
#include "ios/web/js_messaging/web_frame_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a JavaScript safe string based on |script_filename|. This is used as
// a unique identifier for a given script and passed to
// |MakeScriptInjectableOnce| which ensures JS isn't executed multiple times due
// to duplicate injection.
NSString* InjectionTokenForScript(NSString* script_filename) {
  NSMutableCharacterSet* validCharacters =
      [NSMutableCharacterSet alphanumericCharacterSet];
  [validCharacters addCharactersInString:@"$_"];
  NSCharacterSet* invalidCharacters = validCharacters.invertedSet;
  NSString* token =
      [script_filename stringByTrimmingCharactersInSet:invalidCharacters];
  DCHECK_GT(token.length, 0ul);
  return token;
}

}  // namespace

namespace web {

#pragma mark - JavaScriptFeature::FeatureScript

JavaScriptFeature::FeatureScript
JavaScriptFeature::FeatureScript::CreateWithFilename(
    const std::string& filename,
    InjectionTime injection_time,
    TargetFrames target_frames,
    ReinjectionBehavior reinjection_behavior) {
  return JavaScriptFeature::FeatureScript(filename, injection_time,
                                          target_frames, reinjection_behavior);
}

JavaScriptFeature::FeatureScript::FeatureScript(
    const std::string& filename,
    InjectionTime injection_time,
    TargetFrames target_frames,
    ReinjectionBehavior reinjection_behavior)
    : script_filename_(filename),
      injection_time_(injection_time),
      target_frames_(target_frames),
      reinjection_behavior_(reinjection_behavior) {}

JavaScriptFeature::FeatureScript::~FeatureScript() = default;

NSString* JavaScriptFeature::FeatureScript::GetScriptString() const {
  NSString* script_filename = base::SysUTF8ToNSString(script_filename_);
  if (reinjection_behavior_ ==
      ReinjectionBehavior::kReinjectOnDocumentRecreation) {
    return GetPageScript(script_filename);
  }
  // WKUserScript instances will automatically be re-injected by WebKit when the
  // document is re-created, even though the JavaScript context will not be
  // re-created. So the script needs to be wrapped in |MakeScriptInjectableOnce|
  // so that is is not re-injected.
  return MakeScriptInjectableOnce(InjectionTokenForScript(script_filename),
                                  GetPageScript(script_filename));
}

#pragma mark - JavaScriptFeature

JavaScriptFeature::JavaScriptFeature(ContentWorld supported_world)
    : supported_world_(supported_world), weak_factory_(this) {}

JavaScriptFeature::JavaScriptFeature(
    ContentWorld supported_world,
    std::vector<const FeatureScript> feature_scripts)
    : supported_world_(supported_world),
      scripts_(feature_scripts),
      weak_factory_(this) {}

JavaScriptFeature::JavaScriptFeature(
    ContentWorld supported_world,
    std::vector<const FeatureScript> feature_scripts,
    std::vector<const JavaScriptFeature*> dependent_features)
    : supported_world_(supported_world),
      scripts_(feature_scripts),
      dependent_features_(dependent_features),
      weak_factory_(this) {}

JavaScriptFeature::~JavaScriptFeature() = default;

JavaScriptFeature::ContentWorld JavaScriptFeature::GetSupportedContentWorld()
    const {
  return supported_world_;
}

const std::vector<const JavaScriptFeature::FeatureScript>
JavaScriptFeature::GetScripts() const {
  return scripts_;
}

const std::vector<const JavaScriptFeature*>
JavaScriptFeature::GetDependentFeatures() const {
  return dependent_features_;
}

std::vector<std::string> JavaScriptFeature::GetScriptMessageHandlerNames()
    const {
  return {};
}

std::map<std::string, JavaScriptFeature::ScriptMessageHandler>
JavaScriptFeature::GetScriptMessageHandlers() const {
  auto handler = base::BindRepeating(&JavaScriptFeature::ScriptMessageReceived,
                                     weak_factory_.GetWeakPtr());
  auto handlers =
      std::map<std::string, JavaScriptFeature::ScriptMessageHandler>();
  for (auto handler_name : GetScriptMessageHandlerNames()) {
    handlers[handler_name] = handler;
  }
  return handlers;
}

void JavaScriptFeature::ScriptMessageReceived(BrowserState* browser_state,
                                              WKScriptMessage* message) {}

bool JavaScriptFeature::CallJavaScriptFunction(
    WebFrame* web_frame,
    const std::string& function_name,
    const std::vector<base::Value>& parameters) {
  WebFrameImpl* web_frame_impl = static_cast<WebFrameImpl*>(web_frame);
  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(
          web_frame_impl->GetWebState()->GetBrowserState());
  DCHECK(feature_manager);
  JavaScriptContentWorld* content_world =
      feature_manager->GetContentWorldForFeature(this);
  // A feature can still ExecuteJavaScript even if there are no initial scripts,
  // so a nil content_world here will execute JS in the main page content world.
  return web_frame_impl->CallJavaScriptFunction(function_name, parameters,
                                                content_world);
}

}  // namespace web
