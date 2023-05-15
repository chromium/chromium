// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_MANAGER_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_MANAGER_H_

#import <map>
#import <set>
#import <string>
#import <vector>

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace content {
class RenderFrameHost;
}

namespace js_injection {
class JsCommunicationHost;
}

namespace web {

class ScriptMessage;

// Configures JavaScriptFeatures, by injecting document start and end scripts,
// and owning a mapping for routing script message callbacks.
class ContentJavaScriptFeatureManager {
 public:
  explicit ContentJavaScriptFeatureManager(
      std::vector<JavaScriptFeature*> features);
  ~ContentJavaScriptFeatureManager();

  ContentJavaScriptFeatureManager(const ContentJavaScriptFeatureManager&) =
      delete;
  ContentJavaScriptFeatureManager& operator=(
      const ContentJavaScriptFeatureManager&) = delete;

  // Adds document start scripts for the configured features, to the given
  // `js_communication_host`.
  void AddDocumentStartScripts(
      js_injection::JsCommunicationHost* js_communication_host);

  // Injects document end scripts for the configured features, into the given
  // `render_frame_host`.
  void InjectDocumentEndScripts(content::RenderFrameHost* render_frame_host);

  // Returns true if this feature manager already has the given `feature`.
  bool HasFeature(const JavaScriptFeature* feature) const;

  // Handles a `script_message` from JavaScript in `web_state`, directed to the
  // given `handler_name`
  void ScriptMessageReceived(const ScriptMessage& script_message,
                             std::string handler_name,
                             WebState* web_state);

 private:
  // Adds the given `feature` to the set of features managed by this feature
  // manager, unless the given `feature` has already been added.
  void AddFeature(const JavaScriptFeature* feature);

  // The features which are managed by this feature manager.
  std::set<const JavaScriptFeature*> features_;

  // Maps handler names to message handlers.
  std::map<std::string, JavaScriptFeature::ScriptMessageHandler>
      script_message_handlers_;

  // Scripts that are injected when the document element is created.
  std::vector<std::u16string> document_start_scripts_;

  // Scripts that are injected after the document has loaded.
  std::vector<std::u16string> document_end_scripts_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_MANAGER_H_
