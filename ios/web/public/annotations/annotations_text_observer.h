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
// `seq_id` is needed for any calls to
// `AnnotationsTextManager::DecorateAnnotations`, to make sure the annotations
// apply to the same text extracted (which can, for example, be replaced by
// translate).
class AnnotationsTextObserver : public base::CheckedObserver {
 public:
  // Called on page load, after `text` has been extracted.
  // 'metadata' contains the following key/pair values:
  // Bool 'hasNoIntentDetection': true when web page has requested
  //   'nointentdetection'.
  // Bool 'hasNoTranslate': true when web page has
  //   requested 'notranslate'.
  // String 'htmlLang': value from document.documentElement.lang or
  //   "".
  // String 'httpContentLanguage': value of content from
  //   <meta http-equiv="content-language" content="en"> or "".
  // String 'wkNoTelephone': true if the page header contains webkit's:
  //   <meta name="format-detection" content="telephone=no">
  // String 'wkNoEmail': true if the page header contains webkit's:
  //   <meta name="format-detection" content="email=no">
  // String 'wkNoAddress': true if the page header contains webkit's:
  //   <meta name="format-detection" content="address=no">
  // String 'wkNoDate': true if the page header contains webkit's:
  //   <meta name="format-detection" content="date=no">
  // String 'wkNoUnits': true if the page header contains webkit's:
  //   <meta name="format-detection" content="unit=no">
  // Note all type=equal pairs can be also be comma separated in a single
  // content attribute in a meta tag. The check is case insensitive but the
  // metadata is guaranteed lowercase.
  virtual void OnTextExtracted(WebState* web_state,
                               const std::string& text,
                               int seq_id,
                               const base::Value::Dict& metadata) {}

  // Called when decorations have been applied. `successes` is the number of
  // annotations that were successfully stylized in the page, reversely
  // `failures` is the number of annotation that failed to decorate.
  // `annotations` is the number of annotations that were sent for decorating.
  // `cancelled` is the list of ids (`data`) of annotations that fully
  // failed to decorate. There no guarantee that
  //   failures + successes == annotations
  //   failures == cancelled.length
  // because an annotation can be partially decorated due to some changes in the
  // web page. There is also no guarantee this will be called.
  virtual void OnDecorated(WebState* web_state,
                           int annotations,
                           int successes,
                           int failures,
                           const base::Value::List& cancelled) {}

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
