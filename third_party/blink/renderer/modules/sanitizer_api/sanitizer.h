// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_

#include "third_party/blink/renderer/core/dom/node.h"
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

enum ElementKind {
  kCustom,
  kUnknown,
  kRegular,
};

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
  Node* DropElement(Node*, DocumentFragment*);
  Node* BlockElement(Node*, DocumentFragment*, ExceptionState&);
  Node* KeepElement(Node*, DocumentFragment*, String&);

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

  bool allow_custom_elements_ = false;

  bool has_allow_elements_ = false;
  bool has_allow_attributes_ = false;

  // TODO(lyf): make it all-ailpan.
  const HashSet<String> default_block_elements_ = {};
  const HashSet<String> default_drop_elements_ = {};
  const HashSet<String> default_allow_elements_ = {
      "A",          "ABBR",    "ACRONYM", "ADDRESS",  "AREA",     "ARTICLE",
      "ASIDE",      "AUDIO",   "B",       "BDI",      "BDO",      "BIG",
      "BLOCKQUOTE", "BODY",    "BR",      "BUTTON",   "CANVAS",   "CAPTION",
      "CENTER",     "CITE",    "CODE",    "COL",      "COLGROUP", "DATALIST",
      "DD",         "DEL",     "DETAILS", "DIALOG",   "DFN",      "DIR",
      "DIV",        "DL",      "DT",      "EM",       "FIELDSET", "FIGCAPTION",
      "FIGURE",     "FONT",    "FOOTER",  "FORM",     "H1",       "H2",
      "H3",         "H4",      "H5",      "H6",       "HEAD",     "HEADER",
      "HGROUP",     "HR",      "HTML",    "I",        "IMG",      "INPUT",
      "INS",        "KBD",     "KEYGEN",  "LABEL",    "LEGEND",   "LI",
      "LINK",       "LISTING", "MAP",     "MARK",     "MENU",     "META",
      "METER",      "NAV",     "NOBR",    "NOSCRIPT", "OL",       "OPTGROUP",
      "OPTION",     "OUTPUT",  "P",       "PICTURE",  "PRE",      "PROGRESS",
      "Q",          "RB",      "RP",      "RT",       "RTC",      "RUBY",
      "S",          "SAMP",    "SECTION", "SELECT",   "SLOT",     "SMALL",
      "SOURCE",     "SPAN",    "STRIKE",  "STRONG",   "SUB",      "SUMMARY",
      "SUP",        "STYLE",   "TABLE",   "TBODY",    "TD",       "TEXTAREA",
      "TFOOT",      "TH",      "THEAD",   "TIME",     "TR",       "TRACK",
      "TT",         "U",       "UL",      "VAR",      "VIDEO",    "WBR"};
  const Vector<String> kVectorStar = Vector<String>({"*"});
  const HashMap<String, Vector<String>> default_drop_attributes_ = {
      {"onabort", kVectorStar},
      {"onafterprint", kVectorStar},
      {"onanimationstart", kVectorStar},
      {"onanimationiteration", kVectorStar},
      {"onanimationend", kVectorStar},
      {"onauxclick", kVectorStar},
      {"onbeforecopy", kVectorStar},
      {"onbeforecut", kVectorStar},
      {"onbeforepaste", kVectorStar},
      {"onbeforeprint", kVectorStar},
      {"onbeforeunload", kVectorStar},
      {"onblur", kVectorStar},
      {"oncancel", kVectorStar},
      {"oncanplay", kVectorStar},
      {"oncanplaythrough", kVectorStar},
      {"onchange", kVectorStar},
      {"onclick", kVectorStar},
      {"onclose", kVectorStar},
      {"oncontextmenu", kVectorStar},
      {"oncopy", kVectorStar},
      {"oncuechange", kVectorStar},
      {"oncut", kVectorStar},
      {"ondblclick", kVectorStar},
      {"ondrag", kVectorStar},
      {"ondragend", kVectorStar},
      {"ondragenter", kVectorStar},
      {"ondragleave", kVectorStar},
      {"ondragover", kVectorStar},
      {"ondragstart", kVectorStar},
      {"ondrop", kVectorStar},
      {"ondurationchange", kVectorStar},
      {"onemptied", kVectorStar},
      {"onended", kVectorStar},
      {"onerror", kVectorStar},
      {"onfocus", kVectorStar},
      {"onfocusin", kVectorStar},
      {"onfocusout", kVectorStar},
      {"onformdata", kVectorStar},
      {"ongotpointercapture", kVectorStar},
      {"onhashchange", kVectorStar},
      {"oninput", kVectorStar},
      {"oninvalid", kVectorStar},
      {"onkeydown", kVectorStar},
      {"onkeypress", kVectorStar},
      {"onkeyup", kVectorStar},
      {"onlanguagechange", kVectorStar},
      {"onload", kVectorStar},
      {"onloadeddata", kVectorStar},
      {"onloadedmetadata", kVectorStar},
      {"onloadstart", kVectorStar},
      {"onlostpointercapture", kVectorStar},
      {"onmessage", kVectorStar},
      {"onmessageerror", kVectorStar},
      {"onmousedown", kVectorStar},
      {"onmouseenter", kVectorStar},
      {"onmouseleave", kVectorStar},
      {"onmousemove", kVectorStar},
      {"onmouseout", kVectorStar},
      {"onmouseover", kVectorStar},
      {"onmouseup", kVectorStar},
      {"onmousewheel", kVectorStar},
      {"ononline", kVectorStar},
      {"onoffline", kVectorStar},
      {"onorientationchange", kVectorStar},
      {"onoverscroll", kVectorStar},
      {"onpagehide", kVectorStar},
      {"onpageshow", kVectorStar},
      {"onpaste", kVectorStar},
      {"onpause", kVectorStar},
      {"onplay", kVectorStar},
      {"onplaying", kVectorStar},
      {"onpointercancel", kVectorStar},
      {"onpointerdown", kVectorStar},
      {"onpointerenter", kVectorStar},
      {"onpointerleave", kVectorStar},
      {"onpointermove", kVectorStar},
      {"onpointerout", kVectorStar},
      {"onpointerover", kVectorStar},
      {"onpointerrawupdate", kVectorStar},
      {"onpointerup", kVectorStar},
      {"onpopstate", kVectorStar},
      {"onportalactivate", kVectorStar},
      {"onprogress", kVectorStar},
      {"onratechange", kVectorStar},
      {"onreset", kVectorStar},
      {"onresize", kVectorStar},
      {"onscroll", kVectorStar},
      {"onscrollend", kVectorStar},
      {"onsearch", kVectorStar},
      {"onseeked", kVectorStar},
      {"onseeking", kVectorStar},
      {"onselect", kVectorStar},
      {"onselectstart", kVectorStar},
      {"onselectionchange", kVectorStar},
      {"onshow", kVectorStar},
      {"onstalled", kVectorStar},
      {"onstorage", kVectorStar},
      {"onsuspend", kVectorStar},
      {"onsubmit", kVectorStar},
      {"ontimeupdate", kVectorStar},
      {"ontimezonechange", kVectorStar},
      {"ontoggle", kVectorStar},
      {"ontouchstart", kVectorStar},
      {"ontouchmove", kVectorStar},
      {"ontouchend", kVectorStar},
      {"ontouchcancel", kVectorStar},
      {"ontransitionend", kVectorStar},
      {"onunload", kVectorStar},
      {"onvolumechange", kVectorStar},
      {"onwaiting", kVectorStar},
      {"onwebkitanimationstart", kVectorStar},
      {"onwebkitanimationiteration", kVectorStar},
      {"onwebkitanimationend", kVectorStar},
      {"onwebkitfullscreenchange", kVectorStar},
      {"onwebkitfullscreenerror", kVectorStar},
      {"onwebkittransitionend", kVectorStar},
      {"onwheel", kVectorStar}};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
