// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DISALLOW_TRANSITION_SCOPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DISALLOW_TRANSITION_SCOPE_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/web_document.h"

namespace blink {

class DocumentLifecycle;

class WebDisallowTransitionScope {
  // Causes DCHECKs only, does not actually prevent lifecycle changes.
  // This is useful to prevent certain types of crashes that occur, for example,
  // when updating properties in the accessible object hierarchy.
 public:
  BLINK_EXPORT explicit WebDisallowTransitionScope(WebDocument* web_document);
  BLINK_EXPORT virtual ~WebDisallowTransitionScope();

 private:
  DocumentLifecycle& Lifecycle(WebDocument*) const;

  DocumentLifecycle& document_lifecycle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DISALLOW_TRANSITION_SCOPE_H_
