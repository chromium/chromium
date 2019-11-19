// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_HTTP_EQUIV_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_HTTP_EQUIV_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class Element;

/**
 * Handles a HTTP header equivalent set by a meta tag using
 * <meta http-equiv="..." content="...">. This is called when a meta tag is
 * encountered during document parsing, and also when a script dynamically
 * changes or adds a meta tag. This enables scripts to use meta tags to perform
 * refreshes and set expiry dates in addition to them being specified in a HTML
 * file.
 */
class HttpEquiv {
  STATIC_ONLY(HttpEquiv);

 public:
  static void Process(Document&,
                      const AtomicString& equiv,
                      const AtomicString& content,
                      bool in_document_head_element,
                      Element*);

 private:
  static void ProcessHttpEquivDefaultStyle(Document&,
                                           const AtomicString& content);
  static void ProcessHttpEquivRefresh(Document&,
                                      const AtomicString& content,
                                      Element*);
  static void ProcessHttpEquivSetCookie(Document&,
                                        const AtomicString& content,
                                        Element*);
  static void ProcessHttpEquivContentSecurityPolicy(
      Document&,
      const AtomicString& equiv,
      const AtomicString& content);
  static void ProcessHttpEquivAcceptCH(Document&, const AtomicString& content);
  static void ProcessHttpEquivAcceptCHLifetime(Document&,
                                               const AtomicString& content);
};

}  // namespace blink

#endif
