// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_ANNOTATIONS_ANNOTATIONS_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_ANNOTATIONS_ANNOTATIONS_JAVA_SCRIPT_FEATURE_H_

#import <optional>
#import <string>

#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class AnnotationTextManagerTest;

extern const int kMaxAnnotationsTextLength;
extern const int kMaxAnnotationsMetadataLength;

/**
 * Handles JS communication for the annotations feature.
 */
class AnnotationsJavaScriptFeature : public JavaScriptFeature {
 public:
  static AnnotationsJavaScriptFeature* GetInstance();
  static void SetInstanceForTesting(AnnotationsJavaScriptFeature* instance);

  ~AnnotationsJavaScriptFeature() override;

  // Triggers the JS text extraction code. Async calls `OnTextExtracted` on
  // `AnnotationsTextManager` when done using provided `seq_id`.

  virtual void ExtractText(WebState* web_state,
                           int maximum_text_length,
                           int seq_id);

  // Triggers the JS decoration code with given `annotations`. Async calls
  // `OnDecorated` on `AnnotationsTextManager` when done. Decorations will also
  // call `OnClick` on `AnnotationsTextManager` when tapped on.
  virtual void DecorateAnnotations(WebState* web_state,
                                   base::Value& annotations,
                                   int seq_id);
  // Triggers the JS decoration removal code.
  virtual void RemoveDecorations(WebState* web_state);
  // Triggers the JS decoration removal code for a single type.
  virtual void RemoveDecorationsWithType(WebState* web_state,
                                         const std::string& type);

 protected:
  // JavaScriptFeature:
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& script_message) override;
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  AnnotationsJavaScriptFeature();

 private:
  friend class base::NoDestructor<AnnotationsJavaScriptFeature>;
  friend class AnnotationTextManagerTest;

  // Constructor that allows disabling trusted event checks, e.g. for testing.
  explicit AnnotationsJavaScriptFeature(bool trusted_event_check_enabled);

  AnnotationsJavaScriptFeature(const AnnotationsJavaScriptFeature&) = delete;
  AnnotationsJavaScriptFeature& operator=(const AnnotationsJavaScriptFeature&) =
      delete;

  bool trusted_event_check_enabled_ = true;
};

}  // namespace web

#endif  // IOS_WEB_ANNOTATIONS_ANNOTATIONS_JAVA_SCRIPT_FEATURE_H_
