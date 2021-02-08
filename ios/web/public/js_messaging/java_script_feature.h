// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

@class NSString;
@class WKScriptMessage;

namespace base {
class Value;
}  // namespace base

namespace web {

class BrowserState;
class WebFrame;

// Describes a feature implemented in Javascript and native<->JS communication
// (if any). It is intended to be instantiated directly for simple features
// requiring injection only, but should subclassed into feature specific classes
// to handle JS<->native communication.
// NOTE: As implemented within //ios/web, JavaScriptFeature instances holds no
// state itself and can be used application-wide across browser states. However,
// this is not guaranteed of JavaScriptFeature subclasses.
class JavaScriptFeature {
 public:
  // The content world which this feature supports.
  // NOTE: Features should use kAnyContentWorld whenever possible to allow for
  // isolation between the feature and the loaded webpage JavaScript.
  enum class ContentWorld {
    // Represents any content world.
    kAnyContentWorld = 0,
    // Represents the page content world which is shared by the JavaScript of
    // the webpage. This value should only be used if the feature provides
    // JavaScript which needs to be accessible to the client JavaScript. For
    // example, JavaScript polyfills.
    kPageContentWorld,
  };

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
      // modifications to the DOM and |document| JS object. Note, however, that
      // this option can also overwrite or duplicate state which was already
      // previously added to the window's state.
      kReinjectOnDocumentRecreation,
    };

    // The frames which this script will be injected into.
    enum class TargetFrames {
      kAllFrames = 0,
      kMainFrame,
    };

    // Creates a FeatureScript with the script file from the application bundle
    // with |filename| to be injected at |injection_time| into |target_frames|
    // using |reinjection_behavior|.
    static FeatureScript CreateWithFilename(
        const std::string& filename,
        InjectionTime injection_time,
        TargetFrames target_frames,
        ReinjectionBehavior reinjection_behavior =
            ReinjectionBehavior::kInjectOncePerWindow);

    // Returns the JavaScript string of the script with |script_filename_|.
    NSString* GetScriptString() const;

    InjectionTime GetInjectionTime() const { return injection_time_; }
    TargetFrames GetTargetFrames() const { return target_frames_; }

    ~FeatureScript();

   private:
    FeatureScript(const std::string& filename,
                  InjectionTime injection_time,
                  TargetFrames target_frames,
                  ReinjectionBehavior reinjection_behavior);

    std::string script_filename_;
    InjectionTime injection_time_;
    TargetFrames target_frames_;
    ReinjectionBehavior reinjection_behavior_;
  };

  JavaScriptFeature(ContentWorld supported_world,
                    std::vector<const FeatureScript> feature_scripts);
  JavaScriptFeature(ContentWorld supported_world,
                    std::vector<const FeatureScript> feature_scripts,
                    std::vector<const JavaScriptFeature*> dependent_features);
  virtual ~JavaScriptFeature();

  // Returns the supported content world for this feature.
  ContentWorld GetSupportedContentWorld() const;

  // Returns a vector of scripts used by this feature.
  virtual const std::vector<const FeatureScript> GetScripts() const;
  // Returns a vector of features which this one depends upon being available.
  virtual const std::vector<const JavaScriptFeature*> GetDependentFeatures()
      const;

  // Returns the script message handler names which this feature will receive
  // messages from JavaScript. Returning an empty vector will not register any
  // handlers.
  virtual std::vector<std::string> GetScriptMessageHandlerNames() const;

  using ScriptMessageHandler =
      base::RepeatingCallback<void(BrowserState* browser_state,
                                   WKScriptMessage* message)>;
  // Returns the names from |GetScriptMessageHandlerNames| mapped to handler
  // callbacks.
  std::map<std::string, ScriptMessageHandler> GetScriptMessageHandlers() const;

  JavaScriptFeature(const JavaScriptFeature&) = delete;

 protected:
  explicit JavaScriptFeature(ContentWorld supported_world);

  bool CallJavaScriptFunction(WebFrame* web_frame,
                              const std::string& function_name,
                              const std::vector<base::Value>& parameters);

  bool CallJavaScriptFunction(
      WebFrame* web_frame,
      const std::string& function_name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout);

  // Callback for script messages registered through |GetScriptMessageHandlers|.
  // Called when a web view associated with |browser_state| sent |message|.
  virtual void ScriptMessageReceived(BrowserState* browser_state,
                                     WKScriptMessage* message);

 private:
  ContentWorld supported_world_;
  std::vector<const FeatureScript> scripts_;
  std::vector<const JavaScriptFeature*> dependent_features_;
  base::WeakPtrFactory<JavaScriptFeature> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_H_
