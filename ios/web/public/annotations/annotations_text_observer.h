// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_OBSERVER_H_
#define IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_OBSERVER_H_

#include "base/observer_list.h"

namespace web {
class WebState;

// Allows observation of web page text extracted, for annotations or other
// purpose (like language detection). All methods are called on main thread.
class AnnotationsTextObserver : public base::CheckedObserver {
 public:
  // Called on page load, after `text` has been extracted.
  virtual void OnTextExtracted(WebState* web_state, const std::string& text) {}

  // Called when decorations have been applied. `successes` is the number of
  // annotations that were successfully stylized in the page. `annotations` is
  // the number of annotations that were sent for decorating.
  virtual void OnDecorated(WebState* web_state,
                           int successes,
                           int annotations) {}

  // Called when user taps an annotation. `text` is the original annotation
  // text, `rect` is the position in the web page where the annotation is and
  // `data` is the encoded data attached to each annotation.
  virtual void OnClick(WebState* web_state,
                       const std::string& text,
                       CGRect rect,
                       const std::string& data) {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_OBSERVER_H_
