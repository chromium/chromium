// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_

#import <Foundation/Foundation.h>

#import <optional>
#include <string>
#include <vector>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/origin_filter.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace web {

class FuzzerEnvWithJavaScriptFeature;
class ScriptMessage;
class WebState;
class WebFrame;
class WebFramesManager;

// Describes a feature implemented in Javascript and native<->JS communication
// (if any). It is intended to be instantiated directly for simple features
// requiring injection only, but should subclassed into feature specific classes
// to handle JS<->native communication.
// NOTE: As implemented within //ios/web, JavaScriptFeature instances holds no
// state itself and can be used application-wide across browser states. However,
// this is not guaranteed of JavaScriptFeature subclasses.
class JavaScriptFeature {
  friend class ContentJavaScriptFeatureManager;
  // `FuzzerEnvWithJavaScriptFeature` stores subclasses of `JavaScriptFeature`
  // and invokes `ScriptMessageReceived` function in a public API. So fuzzers
  // can call `ScriptMessageReceived` functions without friending with each
  // subclass.
  friend class FuzzerEnvWithJavaScriptFeature;

 public:
  // A script to be injected into webpage frames which support this feature.
  class FeatureScript {
   public:
    // The time at which this script will be injected into the page.
    enum class InjectionTime {
      kDocumentStart = 0,
      kDocumentEnd,
    };

    // Describes whether or not this script should be re-injected when the
    // document is re-created.
    enum class ReinjectionBehavior {
      // The script will only be injected once per window.
      kInjectOncePerWindow = 0,
      // The script will be re-injected when the document is re-created.
      // NOTE: This is necessary to re-add event listeners and to re-inject
      // modifications to the DOM and `document` JS object. Note, however, that
      // this option can also overwrite or duplicate state which was already
      // previously added to the window's state.
      kReinjectOnDocumentRecreation,
    };

    // The frames which this script will be injected into.
    enum class TargetFrames {
      kAllFrames = 0,
      kMainFrame,
    };

    // Mapping of placeholder to their replacement value.
    using PlaceholderReplacements = NSDictionary<NSString*, NSString*>*;

    // Callback used to perform placeholder replacement in the script. The
    // returned value is a dictionary mapping "placeholder" to the "value"
    // that needs it to be substituted by with in the script.
    using PlaceholderReplacementsCallback =
        base::RepeatingCallback<PlaceholderReplacements()>;

    // Creates a FeatureScript with the script file from the application bundle
    // with `filename` to be injected at `injection_time` into `target_frames`
    // using `reinjection_behavior`. If `replacements` is provided, it will be
    // used to replace placeholder with the corresponding string values. If
    // `origin_filter` is provided, the script will have no effect in pages from
    // other origins.
    static FeatureScript CreateWithFilename(
        const std::string& filename,
        InjectionTime injection_time,
        TargetFrames target_frames,
        ReinjectionBehavior reinjection_behavior =
            ReinjectionBehavior::kInjectOncePerWindow,
        const PlaceholderReplacementsCallback& replacements_callback =
            PlaceholderReplacementsCallback(),
        OriginFilter origin_filter = OriginFilter::kPublic);

    // Creates a FeatureScript with the string `script` to be injected at
    // `injection_time` into `target_frames` using `reinjection_behavior`. If
    // `replacements` is provided, it will be used to replace placeholder with
    // the corresponding string values. If `origin_filter` is provided, the
    // script will have no effect in pages from other origins.
    static FeatureScript CreateWithString(
        const std::string& script,
        InjectionTime injection_time,
        TargetFrames target_frames,
        ReinjectionBehavior reinjection_behavior =
            ReinjectionBehavior::kInjectOncePerWindow,
        const PlaceholderReplacementsCallback& replacements_callback =
            PlaceholderReplacementsCallback(),
        OriginFilter origin_filter = OriginFilter::kPublic);

    FeatureScript(const FeatureScript& other);
    FeatureScript& operator=(const FeatureScript&);

    FeatureScript(FeatureScript&&);
    FeatureScript& operator=(FeatureScript&&);

    // Returns the JavaScript string of the script with `script_filename_`.
    NSString* GetScriptString() const;

    InjectionTime GetInjectionTime() const { return injection_time_; }
    TargetFrames GetTargetFrames() const { return target_frames_; }
    OriginFilter GetOriginFilter() const { return origin_filter_; }

    ~FeatureScript();

   private:
    FeatureScript(std::optional<std::string> filename,
                  std::optional<std::string> script,
                  NSString* injection_token,
                  InjectionTime injection_time,
                  TargetFrames target_frames,
                  ReinjectionBehavior reinjection_behavior,
                  const PlaceholderReplacementsCallback& replacements_callback,
                  OriginFilter origin_filter);

    // Returns `script` after swapping the placeholders with their value as
    // instructed by `replacements_callback_`.
    NSString* ReplacePlaceholders(NSString* script) const;

    std::optional<std::string> script_filename_;
    std::optional<std::string> script_;
    NSString* injection_token_;
    InjectionTime injection_time_;
    TargetFrames target_frames_;
    ReinjectionBehavior reinjection_behavior_;
    OriginFilter origin_filter_;
    PlaceholderReplacementsCallback replacements_callback_;
  };

  // Constructs a new feature instance inside the world described by
  // `supported_world`. Each FeatureScript within `feature_scripts` will be
  // configured within that same world.
  // NOTE: Features should use `kIsolatedWorld` whenever possible to allow for
  // isolation between the feature and the loaded webpage JavaScript.
  // If `dependent_features` are given, they will be setup in the world
  // specified prior to configuring this feaure. If `origin_filter` is set
  // message will only be processed if coming from this domain. All
  // FeatureScript in `feature_script` must have the same OriginFilter.
  JavaScriptFeature(ContentWorld supported_world,
                    std::vector<FeatureScript> feature_scripts,
                    std::vector<const JavaScriptFeature*> dependent_feature =
                        std::vector<const JavaScriptFeature*>(),
                    OriginFilter origin_filter = OriginFilter::kPublic);
  virtual ~JavaScriptFeature();

  // Returns a weak ptr to the feature.
  base::WeakPtr<JavaScriptFeature> AsWeakPtr() const;

  // Returns the supported content world for this feature.
  ContentWorld GetSupportedContentWorld() const;

  // Returns the WebFramesManager associated with `web_state` for the content
  // world which this feature instance has been configured. This ensures that
  // the WebFrames within WebFramesManager match the environment where the
  // scripts of this feature are executed. This is partifularly important if
  // frameIds are used which are not consistent across content worlds.
  // NOTE: This helper only works for features which are defined to live in a
  // specific content world. To obtain a WebFramesManager for a feature that is
  // configured with ContentWorld::kAllContentWorlds, obtain the frames manager
  // from a higher level feature or obtain the WebFramesManager from the
  // WebState and specify the content world directly.
  WebFramesManager* GetWebFramesManager(WebState* web_state);

  // Returns a vector of scripts used by this feature.
  virtual std::vector<FeatureScript> GetScripts() const;
  // Returns a vector of features which this one depends upon being available.
  virtual std::vector<const JavaScriptFeature*> GetDependentFeatures() const;

  // Returns the script message handler name which this feature will receive
  // messages from JavaScript. Returning null will not register any handler.
  virtual std::optional<std::string> GetScriptMessageHandlerName() const;

  // Returns whether the feature replies to messages sent from JavaScript.
  virtual bool GetFeatureRepliesToMessages() const;

  JavaScriptFeature(const JavaScriptFeature&) = delete;

 protected:
  explicit JavaScriptFeature(ContentWorld supported_world);

  // Calls `function_name` with `parameters` in `web_frame` within the content
  // world that this feature has been configured. `web_frame` must not be null.
  // See WebFrame::CallJavaScriptFunction for more details.
  bool CallJavaScriptFunction(WebFrame* web_frame,
                              const std::string& function_name,
                              const base::ListValue& parameters);

  // Calls `function_name` with `parameters` in `web_frame` within the content
  // world that this feature has been configured. `callback` will be called with
  // the return value of the function if it completes within `timeout`.
  // `web_frame` must not be null.
  // See WebFrame::CallJavaScriptFunction for more details.
  bool CallJavaScriptFunction(
      WebFrame* web_frame,
      const std::string& function_name,
      const base::ListValue& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout);

  // Use of this function is DISCOURAGED. Prefer the `CallJavaScriptFunction`
  // family of functions instead to keep the API clear and well defined.
  // Executes `script` in `web_frame` within the content world that this feature
  // has been configured.
  // See WebFrame::ExecuteJavaScript for more details on `callback`.
  bool ExecuteJavaScript(WebFrame* web_frame,
                         const std::u16string& script,
                         ExecuteJavaScriptCallbackWithError callback);

  // Friending JavaScriptContentWorld so it can bind ScriptMessageReceived and
  // ScriptMessageReceivedWithReply.
  friend class JavaScriptContentWorld;

  // Callback for script messages registered through `GetScriptMessageHandler`.
  // `ScriptMessageReceived` is called when `web_state` receives a `message`.
  // `web_state` will always be non-null.
  // Must be overridden in subclass for features that opt out to reply to
  // messages.
  virtual void ScriptMessageReceived(WebState* web_state,
                                     const ScriptMessage& message);

  using ScriptMessageReplyCallback =
      base::OnceCallback<void(const base::Value* reply, NSString* error)>;

  // Callback for script messages registered through
  // `GetScriptMessageHandlerWithReply`. `ScriptMessageReceived` is called when
  // `web_state` receives a `message`. `web_state` will always be non-null.
  // Must be overridden in subclass for features that opt in to reply to
  // messages.
  virtual void ScriptMessageReceivedWithReply(
      WebState* web_state,
      const ScriptMessage& message,
      ScriptMessageReplyCallback callback);

 private:
  // Whether a message coming from a frame showing a page from `origin` should
  // be handled. Always return `YES` for `kPublic` features.
  bool ShouldHandleMessageFromOrigin(const url::Origin& origin);

  ContentWorld supported_world_;
  const std::vector<FeatureScript> scripts_;
  const std::vector<const JavaScriptFeature*> dependent_features_;
  OriginFilter origin_filter_ = OriginFilter::kPublic;
  std::vector<url::Origin> origin_filter_origins_;
  base::WeakPtrFactory<JavaScriptFeature> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_
