// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#import <optional>

#import "base/check_op.h"
#import "base/debug/crash_logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/javascript_flags.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/util/wk_security_origin_util.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/url_conversions.h"

#if BUILDFLAG(ENABLE_IOS_JAVASCRIPT_FLAGS)
#import "base/command_line.h"
#import "base/strings/string_split.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/switches.h"
#endif

namespace web {

namespace {

// String returned to JavaScript if the message cannot be handled correctly.
NSString* const kInternalError = @"Internal error";

WKUserScriptInjectionTime InjectionTimeToWKUserScriptInjectionTime(
    JavaScriptFeature::FeatureScript::InjectionTime injection_time) {
  switch (injection_time) {
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart:
      return WKUserScriptInjectionTimeAtDocumentStart;
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd:
      return WKUserScriptInjectionTimeAtDocumentEnd;
  }
  NOTREACHED();
}

// Returns the WKUserContentController associated with `browser_state`.
// NOTE: Only fetch the WKUserContentController once at construction. Although
// it is not guaranteed to remain constant over the lifetime of the
// application, the entire JavaScriptcontentWorld will be recreated when it
// changes. Calling WKWebViewConfigurationProvider::GetWebViewConfiguration on
// a configuration provider during destruction will cause partial
// re-initialization during tear down.
WKUserContentController* GetUserContentController(BrowserState* browser_state) {
  return WKWebViewConfigurationProvider::FromBrowserState(browser_state)
      .GetWebViewConfiguration()
      .userContentController;
}

// Creates a ScriptMessage from `script_message` using `web_state`.
// Returns std::nullopt if the message is invalid or if the associated
// WebController is missing.
std::optional<ScriptMessage> GetMessage(WKScriptMessage* script_message,
                                        web::WebState* web_state) {
  if (!web_state) {
    return std::nullopt;
  }

  CRWWebController* web_controller =
      web::WebStateImpl::FromWebState(web_state)->GetWebController();
  if (!web_controller) {
    return std::nullopt;
  }

  NSURL* ns_url = script_message.frameInfo.request.URL;
  std::optional<GURL> url;
  if (ns_url) {
    url = net::GURLWithNSURL(ns_url);
  }

  return ScriptMessage(web::ValueResultFromWKResult(script_message.body),
                       web_controller.isUserInteracting,
                       script_message.frameInfo.mainFrame, url);
}

}  // namespace

JavaScriptContentWorld::JavaScriptContentWorld(BrowserState* browser_state,
                                               WKContentWorld* content_world)
    : browser_state_(browser_state),
      user_content_controller_(GetUserContentController(browser_state)),
      content_world_(content_world),
      weak_factory_(this) {
  DCHECK(content_world_);

#if BUILDFLAG(ENABLE_IOS_JAVASCRIPT_FLAGS)
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    int num_flags_enabled = 0;
    bool disable_all_scripts =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kDisableAllInjectedScripts);
    if (disable_all_scripts) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kDisableAllInjectedScripts
                   << "\n###########\n\n";
    }

    bool disable_feature_scripts =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kDisableInjectedFeatureScripts);
    if (disable_feature_scripts) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kDisableInjectedFeatureScripts
                   << "\n###########\n\n";
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kDisableListedScripts)) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kDisableListedScripts
                   << "\n###########\n\n";
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kEnableListedScripts)) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kEnableListedScripts
                   << "\n###########\n\n";
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kDisableListedJavascriptFeatures)) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kDisableListedJavascriptFeatures
                   << "\n###########\n\n";
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            web::switches::kEnableListedJavascriptFeatures)) {
      num_flags_enabled++;
      LOG(WARNING) << "\n\n###########\nFlag set: "
                   << web::switches::kEnableListedJavascriptFeatures
                   << "\n###########\n\n";
    }

    if (num_flags_enabled > 1) {
      LOG(ERROR) << "Multiple JavaScript flags set, results undefined. Ensure "
                    "only one is set and re-run.";
      abort();
    }
  });
#endif
}

WKContentWorld* JavaScriptContentWorld::GetWKContentWorld() {
  return content_world_;
}

JavaScriptContentWorld::~JavaScriptContentWorld() {}

bool JavaScriptContentWorld::HasFeature(const JavaScriptFeature* feature) {
  return features_.contains(feature);
}

void JavaScriptContentWorld::AddFeature(const JavaScriptFeature* feature) {
#if BUILDFLAG(ENABLE_IOS_JAVASCRIPT_FLAGS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          web::switches::kDisableListedJavascriptFeatures)) {
    std::optional<std::string> message_handler_name =
        feature->GetScriptMessageHandlerName();
    if (message_handler_name) {
      auto disable_features_flag =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              web::switches::kDisableListedJavascriptFeatures);
      auto disable_features = base::SplitStringPiece(disable_features_flag, ",",
                                                     base::TRIM_WHITESPACE,
                                                     base::SPLIT_WANT_NONEMPTY);
      if (std::find(disable_features.begin(), disable_features.end(),
                    message_handler_name.value().c_str()) !=
          disable_features.end()) {
        // `feature`'s message handler name was found in passed switch value.
        return;
      }
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          web::switches::kEnableListedJavascriptFeatures)) {
    std::optional<std::string> message_handler_name =
        feature->GetScriptMessageHandlerName();
    if (feature != java_script_features::GetBaseJavaScriptFeature()) {
      if (!message_handler_name) {
        return;
      }

      auto enable_features_flag =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              web::switches::kEnableListedJavascriptFeatures);
      auto enable_features = base::SplitStringPiece(enable_features_flag, ",",
                                                    base::TRIM_WHITESPACE,
                                                    base::SPLIT_WANT_NONEMPTY);
      if (std::find(enable_features.begin(), enable_features.end(),
                    message_handler_name.value().c_str()) ==
          enable_features.end()) {
        // `feature`'s message handler name was NOT found in passed switch
        // value.
        return;
      }
    }
  }
#endif

  if (HasFeature(feature)) {
    // `feature` has already been added to this content world.
    return;
  }

  // Ensure `feature` supports this `content_world_`.
  if (content_world_ == WKContentWorld.pageWorld) {
    // A feature specifying kIsolatedWorld can not be added to the page
    // content world.
    DCHECK_NE(feature->GetSupportedContentWorld(),
              ContentWorld::kIsolatedWorld);
  } else {
    // A feature specifying kPageContentWorld can not be added to an isolated
    // world.
    DCHECK_NE(feature->GetSupportedContentWorld(),
              ContentWorld::kPageContentWorld);
  }

  features_.insert(feature);

  // Add dependent features first.
  for (const JavaScriptFeature* dep_feature : feature->GetDependentFeatures()) {
    AddFeature(dep_feature);
  }

  // Setup user scripts.
  for (const JavaScriptFeature::FeatureScript& feature_script :
       feature->GetScripts()) {
    WKUserScriptInjectionTime injection_time =
        InjectionTimeToWKUserScriptInjectionTime(
            feature_script.GetInjectionTime());

    bool main_frame_only =
        feature_script.GetTargetFrames() !=
        JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;

    if (content_world_) {
      WKUserScript* user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only
                                inContentWorld:content_world_];
      [user_content_controller_ addUserScript:user_script];
    } else {
      WKUserScript* user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only];
      [user_content_controller_ addUserScript:user_script];
    }
  }

  // Setup Javascript message callback.
  auto optional_handler_name = feature->GetScriptMessageHandlerName();
  if (optional_handler_name) {
    NSString* handler_name =
        base::SysUTF8ToNSString(optional_handler_name.value());
    std::unique_ptr<ScopedWKScriptMessageHandler> script_message_handler;
    if (content_world_) {
      if (feature->GetFeatureRepliesToMessages()) {
        script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
            user_content_controller_, handler_name, content_world_,
            base::BindRepeating(
                &JavaScriptContentWorld::ScriptMessageReceivedWithReply,
                weak_factory_.GetWeakPtr(), feature->AsWeakPtr()));
      } else {
        script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
            user_content_controller_, handler_name, content_world_,
            base::BindRepeating(&JavaScriptContentWorld::ScriptMessageReceived,
                                weak_factory_.GetWeakPtr(),
                                feature->AsWeakPtr()));
      }
    } else {
      CHECK(!feature->GetFeatureRepliesToMessages());
      script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
          user_content_controller_, handler_name,
          base::BindRepeating(&JavaScriptContentWorld::ScriptMessageReceived,
                              weak_factory_.GetWeakPtr(),
                              feature->AsWeakPtr()));
    }
    script_message_handlers_[feature] = std::move(script_message_handler);
  }
}

void JavaScriptContentWorld::ScriptMessageReceived(
    base::WeakPtr<JavaScriptFeature> feature,
    WKScriptMessage* script_message) {
  SCOPED_CRASH_KEY_STRING32("ScriptMessage", "name",
                            base::SysNSStringToUTF8(script_message.name));
  if (!feature) {
    return;
  }

  web::WebViewWebStateMap* map =
      web::WebViewWebStateMap::FromBrowserState(browser_state_);
  web::WebState* web_state = map->GetWebStateForWebView(script_message.webView);

  if (!web_state) {
    return;
  }
  if (!feature->ShouldHandleMessageFromOrigin(web::OriginWithWKSecurityOrigin(
          script_message.frameInfo.securityOrigin))) {
    return;
  }
  std::optional<ScriptMessage> message = GetMessage(script_message, web_state);
  if (!message) {
    return;
  }

  feature->ScriptMessageReceived(web_state, *message);
}

void JavaScriptContentWorld::ScriptMessageReceivedWithReply(
    base::WeakPtr<JavaScriptFeature> feature,
    WKScriptMessage* script_message,
    ScriptMessageReplyHandler reply_handler) {
  SCOPED_CRASH_KEY_STRING32("ScriptMessage", "name",
                            base::SysNSStringToUTF8(script_message.name));
  CHECK(reply_handler);
  if (!feature) {
    reply_handler(nullptr, kInternalError);
    return;
  }

  web::WebViewWebStateMap* map =
      web::WebViewWebStateMap::FromBrowserState(browser_state_);
  web::WebState* web_state = map->GetWebStateForWebView(script_message.webView);

  if (!web_state) {
    reply_handler(nullptr, kInternalError);
    return;
  }
  if (!feature->ShouldHandleMessageFromOrigin(web::OriginWithWKSecurityOrigin(
          script_message.frameInfo.securityOrigin))) {
    reply_handler(nullptr, kInternalError);
    return;
  }

  std::optional<ScriptMessage> message = GetMessage(script_message, web_state);
  if (!message) {
    reply_handler(nullptr, kInternalError);
    return;
  }

  auto callback = base::BindOnce(
      [](ScriptMessageReplyHandler reply_handler, const base::Value* reply,
         NSString* error) { reply_handler(reply, error); },
      reply_handler);
  feature->ScriptMessageReceivedWithReply(web_state, *message,
                                          std::move(callback));
}

}  // namespace web
