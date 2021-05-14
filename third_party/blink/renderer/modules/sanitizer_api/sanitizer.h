// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class DocumentFragment;
class ExceptionState;
class ExecutionContext;
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
  static Sanitizer* Create(ExecutionContext*,
                           const SanitizerConfig*,
                           ExceptionState&);
  explicit Sanitizer(ExecutionContext*, const SanitizerConfig*);
  ~Sanitizer() override;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  String sanitizeToString(ScriptState* script_state,
                          const V8SanitizerInput* input,
                          ExceptionState& exception_state);
  DocumentFragment* sanitize(ScriptState* script_state,
                             const V8SanitizerInputWithTrustedHTML* input,
                             ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  String sanitizeToString(ScriptState*,
                          StringOrDocumentFragmentOrDocument&,
                          ExceptionState&);
  DocumentFragment* sanitize(ScriptState*,
                             StringOrTrustedHTMLOrDocumentFragmentOrDocument&,
                             ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  SanitizerConfig* config() const;
  static SanitizerConfig* defaultConfig();

  void Trace(Visitor*) const override;

 private:
  Node* DropElement(Node*, DocumentFragment*);
  Node* BlockElement(Node*, DocumentFragment*, ExceptionState&);
  Node* KeepElement(Node*, DocumentFragment*, String&, LocalDOMWindow*);

  void ElementFormatter(HashSet<String>&, const Vector<String>&);
  void AttrFormatter(HashMap<String, Vector<String>>&,
                     const Vector<std::pair<String, Vector<String>>>&);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  DocumentFragment* PrepareFragment(LocalDOMWindow* window,
                                    ScriptState* script_state,
                                    const V8SanitizerInput* input,
                                    ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  DocumentFragment* PrepareFragment(LocalDOMWindow*,
                                    ScriptState*,
                                    StringOrDocumentFragmentOrDocument&,
                                    ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  DocumentFragment* DoSanitizing(DocumentFragment*,
                                 LocalDOMWindow*,
                                 ExceptionState&);
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  DocumentFragment* SanitizeImpl(ScriptState* script_state,
                                 const V8SanitizerInput* input,
                                 ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  DocumentFragment* SanitizeImpl(ScriptState*,
                                 StringOrDocumentFragmentOrDocument&,
                                 ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  SanitizerConfigImpl config_;
  Member<const SanitizerConfig> config_dictionary_;

  // TODO(lyf): make it all-oilpan.
  const Vector<String> kVectorStar = Vector<String>({"*"});
  const HashSet<String> baseline_drop_elements_ = {
      "APPLET",   "BASE",    "EMBED",    "IFRAME", "NOEMBED",
      "NOFRAMES", "NOLAYER", "NOSCRIPT", "OBJECT", "FRAME",
      "FRAMESET", "PARAM",   "SCRIPT"};
  const HashMap<String, Vector<String>> baseline_drop_attributes_ = {
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
