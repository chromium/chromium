// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frames_manager.h"

#import <set>

#import "base/no_destructor.h"
#import "base/strings/utf_string_conversions.h"
#import "base/unguessable_token.h"
#import "components/js_injection/browser/js_communication_host.h"
#import "content/public/browser/navigation_handle.h"
#import "content/public/browser/page.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/js_messaging/content_java_script_feature_manager.h"
#import "ios/web/content/js_messaging/content_java_script_feature_util.h"
#import "ios/web/content/js_messaging/content_web_frame.h"
#import "ios/web/content/js_messaging/ios_web_message_host_factory.h"
#import "ios/web/content/web_state/content_web_state.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_client.h"

namespace web {

namespace {

// Names of JSON dictionary properties that JavaScript populates when sending
// messages to the browser.
const char kHandlerNamePropertyName[] = "handler_name";
const char kMessagePropertyName[] = "message";

const char kSendWebKitMessageScriptName[] = "send_webkit_message";

// This feature intercepts calls to window.webkit.messageHandlers and reroutes
// them to window.webkitMessageHandler.
JavaScriptFeature* GetSendWebKitMessageJavaScriptFeature() {
  // Static storage is ok for `send_webkit_message_feature` as it holds no
  // state.
  static base::NoDestructor<JavaScriptFeature> send_webkit_message_feature(
      ContentWorld::kPageContentWorld,
      std::vector<JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kSendWebKitMessageScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}));
  return send_webkit_message_feature.get();
}

}  // namespace

ContentWebFramesManager::ContentWebFramesManager(
    ContentWebState* content_web_state)
    : content::WebContentsObserver(content_web_state->GetWebContents()),
      content_web_state_(content_web_state),
      js_communication_host_(
          std::make_unique<js_injection::JsCommunicationHost>(
              content_web_state->GetWebContents())) {
  auto web_message_callback =
      base::BindRepeating(&ContentWebFramesManager::ScriptMessageReceived,
                          weak_factory_.GetWeakPtr());
  auto message_host_factory =
      std::make_unique<IOSWebMessageHostFactory>(web_message_callback);
  js_communication_host_->AddWebMessageHostFactory(
      std::move(message_host_factory), u"webkitMessageHandler", {"*"});

  std::vector<JavaScriptFeature*> java_script_features;
  java_script_features.push_back(GetSendWebKitMessageJavaScriptFeature());
  for (JavaScriptFeature* feature :
       java_script_features::GetBuiltInJavaScriptFeaturesForContent(
           content_web_state->GetBrowserState())) {
    java_script_features.push_back(feature);
  }
  for (JavaScriptFeature* feature : GetWebClient()->GetJavaScriptFeatures(
           content_web_state->GetBrowserState())) {
    java_script_features.push_back(feature);
  }

  js_feature_manager_ = std::make_unique<ContentJavaScriptFeatureManager>(
      std::move(java_script_features));
  js_feature_manager_->AddDocumentStartScripts(js_communication_host_.get());
}

ContentWebFramesManager::~ContentWebFramesManager() = default;

void ContentWebFramesManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentWebFramesManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::set<WebFrame*> ContentWebFramesManager::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : available_frame_hosts_) {
    frames.insert(WebFrameForContentId(it));
  }
  return frames;
}

WebFrame* ContentWebFramesManager::GetMainWebFrame() {
  auto web_id_it = content_to_web_id_map_.find(main_frame_content_id_);
  if (web_id_it == content_to_web_id_map_.end()) {
    return nullptr;
  }

  return GetFrameWithId(web_id_it->second);
}

WebFrame* ContentWebFramesManager::GetFrameWithId(const std::string& frame_id) {
  if (frame_id.empty()) {
    return nullptr;
  }
  auto web_frames_it = web_frames_.find(frame_id);
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

void ContentWebFramesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  std::string web_frame_id = base::UnguessableToken::Create().ToString();
  auto web_frame = std::make_unique<ContentWebFrame>(
      web_frame_id, render_frame_host, content_web_state_);
  web_frames_[web_frame_id] = std::move(web_frame);
  content_to_web_id_map_[render_frame_host->GetGlobalId()] = web_frame_id;
}

void ContentWebFramesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  content::GlobalRenderFrameHostId content_id =
      render_frame_host->GetGlobalId();
  auto web_id_it = content_to_web_id_map_.find(content_id);
  DCHECK(web_id_it != content_to_web_id_map_.end());

  if (available_frame_hosts_.count(content_id)) {
    for (auto& observer : observers_) {
      observer.WebFrameBecameUnavailable(this, web_id_it->second);
    }
    available_frame_hosts_.erase(content_id);
  }

  if (main_frame_content_id_ == content_id) {
    main_frame_content_id_ = content::GlobalRenderFrameHostId();
  }

  web_frames_.erase(web_id_it->second);
  content_to_web_id_map_.erase(web_id_it);
}

void ContentWebFramesManager::PrimaryPageChanged(content::Page& page) {
  main_frame_content_id_ = page.GetMainDocument().GetGlobalId();
}

void ContentWebFramesManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  content::GlobalRenderFrameHostId content_id =
      render_frame_host->GetGlobalId();
  WebFrame* web_frame = WebFrameForContentId(content_id);

  // Inject JavaScript to override `getFrameId` to return the WebFrame id chosen
  // in `RenderFrameCreated`. This must happen even if the frame has already
  // been added to `available_frame_hosts_`, since navigation to a new document
  // will result in a fresh JavaScript execution context.
  std::u16string format_string = u"__gCrWeb.frameId = '$1';";
  std::u16string script_to_inject = base::ReplaceStringPlaceholders(
      format_string, base::UTF8ToUTF16(web_frame->GetFrameId()),
      /*offset=*/nullptr);
  web_frame->ExecuteJavaScript(script_to_inject);

  js_feature_manager_->InjectDocumentEndScripts(render_frame_host);

  if (available_frame_hosts_.count(content_id)) {
    return;
  }

  available_frame_hosts_.insert(content_id);

  // Notify observers here rather than in `RenderFrameCreated`, to
  // ensure that frames are no longer in a speculative lifecycle
  // phase where JavaScript injection is not yet allowed. crbug.com/1183639
  // tracks delaying `RenderFrameCreated` until frames are past the
  // speculative state, which is not intended to be exposed to embedders.
  for (auto& observer : observers_) {
    observer.WebFrameBecameAvailable(this, web_frame);
  }
}

WebFrame* ContentWebFramesManager::WebFrameForContentId(
    content::GlobalRenderFrameHostId content_id) {
  auto web_id_it = content_to_web_id_map_.find(content_id);
  DCHECK(web_id_it != content_to_web_id_map_.end());
  auto web_frame_it = web_frames_.find(web_id_it->second);
  DCHECK(web_frame_it != web_frames_.end());

  return web_frame_it->second.get();
}

void ContentWebFramesManager::ScriptMessageReceived(
    const ScriptMessage& script_message) {
  // In ios/web/content, only a single script message handler is exposed to
  // JavaScript. To simulate having multiple handlers (as in WKWebView-based
  // ios/web), messages sent to this handler have the form of a dictionary with
  // two fields, specifying the destination handler name along with the actual
  // message for that handler. Once these two parts are extracted from
  // `script_message`, a new ScriptMessage is constructed with only the actual
  // message intended for the handler.

  base::Value::Dict* dict = script_message.body()->GetIfDict();
  if (!dict) {
    return;
  }

  const std::string* handler_name = dict->FindString(kHandlerNamePropertyName);
  base::Value* message_content = dict->Find(kMessagePropertyName);
  if (!handler_name || !message_content) {
    return;
  }

  ScriptMessage message_for_handler(
      std::make_unique<base::Value>(std::move(*message_content)),
      script_message.is_user_interacting(), script_message.is_main_frame(),
      script_message.request_url());

  js_feature_manager_->ScriptMessageReceived(message_for_handler, *handler_name,
                                             content_web_state_);
}

}  // namespace web
