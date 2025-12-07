// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_TESTING_INTERNALS_CONTENT_EXTRACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_TESTING_INTERNALS_CONTENT_EXTRACTION_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class ExceptionState;
class Internals;
class Node;
class String;

class InternalsContentExtraction {
  STATIC_ONLY(InternalsContentExtraction);

 public:
  static String dumpContentNodeTree(Internals&, Document*, ExceptionState&);
  static String dumpContentNode(Internals&, Node*, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_TESTING_INTERNALS_CONTENT_EXTRACTION_H_
