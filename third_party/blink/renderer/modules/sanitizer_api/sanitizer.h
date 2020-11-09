// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class DocumentFragment;
class ExceptionState;
class SanitizerConfig;
class ScriptState;
class StringOrDocumentFragmentOrDocument;
class StringOrTrustedHTMLOrDocumentFragmentOrDocument;

class MODULES_EXPORT Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Sanitizer* Create(const SanitizerConfig*, ExceptionState&);
  explicit Sanitizer(const SanitizerConfig*);
  ~Sanitizer() override;

  String sanitizeToString(ScriptState*,
                          StringOrDocumentFragmentOrDocument&,
                          ExceptionState&);
  DocumentFragment* sanitize(ScriptState*,
                             StringOrTrustedHTMLOrDocumentFragmentOrDocument&,
                             ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void ElementFormatter(HashSet<String>&, const Vector<String>&);
  void AttrFormatter(HashMap<String, Vector<String>>&,
                     const Vector<std::pair<String, Vector<String>>>&);

  DocumentFragment* SanitizeImpl(ScriptState*,
                                 StringOrDocumentFragmentOrDocument&,
                                 ExceptionState&);

  HashSet<String> allow_elements_ = {};
  HashSet<String> block_elements_ = {};
  HashSet<String> drop_elements_ = {};
  HashMap<String, Vector<String>> allow_attributes_ = {};
  HashMap<String, Vector<String>> drop_attributes_ = {};

  bool has_allow_elements_ = false;
  bool has_allow_attributes_ = false;

  const HashSet<String> default_block_elements_ = {};
  const HashSet<String> default_drop_elements_ = {"SCRIPT",    "ANNOTATION-XML",
                                                  "AUDIO",     "COLGROUP",
                                                  "DESC",      "FOREIGNOBJECT",
                                                  "HEAD",      "IFRAME",
                                                  "MATH",      "MI",
                                                  "MN",        "MO",
                                                  "MS",        "MTEXT",
                                                  "NOEMBED",   "NOFRAMES",
                                                  "PLAINTEXT", "STYLE",
                                                  "SVG",       "TEMPLATE",
                                                  "THEAD",     "TITLE",
                                                  "VIDEO",     "XMP"};
  const HashMap<String, Vector<String>> default_drop_attributes_ = {
      {"onclick", Vector<String>({"*"})},
      {"onsubmit", Vector<String>({"*"})}};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
