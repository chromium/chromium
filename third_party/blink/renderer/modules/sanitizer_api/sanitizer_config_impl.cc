// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"

#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

const char* kDefaultAllowElements[] = {
    "a",          "abbr",    "acronym", "address",  "area",     "article",
    "aside",      "audio",   "b",       "bdi",      "bdo",      "big",
    "blockquote", "body",    "br",      "button",   "canvas",   "caption",
    "center",     "cite",    "code",    "col",      "colgroup", "datalist",
    "dd",         "del",     "details", "dialog",   "dfn",      "dir",
    "div",        "dl",      "dt",      "em",       "fieldset", "figcaption",
    "figure",     "font",    "footer",  "form",     "h1",       "h2",
    "h3",         "h4",      "h5",      "h6",       "head",     "header",
    "hgroup",     "hr",      "html",    "i",        "img",      "input",
    "ins",        "kbd",     "keygen",  "label",    "legend",   "li",
    "link",       "listing", "map",     "mark",     "menu",     "meta",
    "meter",      "nav",     "nobr",    "ol",       "optgroup", "option",
    "output",     "p",       "picture", "pre",      "progress", "q",
    "rb",         "rp",      "rt",      "rtc",      "ruby",     "s",
    "samp",       "section", "select",  "small",    "source",   "span",
    "strike",     "strong",  "sub",     "summary",  "sup",      "style",
    "table",      "tbody",   "td",      "textarea", "tfoot",    "th",
    "thead",      "time",    "tr",      "track",    "tt",       "u",
    "ul",         "var",     "video",   "wbr"};

const char* kDefaultAllowAttributes[] = {"abbr",
                                         "accept",
                                         "accept-charset",
                                         "accesskey",
                                         "action",
                                         "align",
                                         "alink",
                                         "allow",
                                         "allowfullscreen",
                                         "alt",
                                         "anchor",
                                         "archive",
                                         "as",
                                         "async",
                                         "autocapitalize",
                                         "autocomplete",
                                         "autocorrect",
                                         "autofocus",
                                         "autopictureinpicture",
                                         "autoplay",
                                         "axis",
                                         "background",
                                         "behavior",
                                         "bgcolor",
                                         "border",
                                         "bordercolor",
                                         "capture",
                                         "cellpadding",
                                         "cellspacing",
                                         "challenge",
                                         "char",
                                         "charoff",
                                         "charset",
                                         "checked",
                                         "cite",
                                         "class",
                                         "classid",
                                         "clear",
                                         "code",
                                         "codebase",
                                         "codetype",
                                         "color",
                                         "cols",
                                         "colspan",
                                         "compact",
                                         "content",
                                         "contenteditable",
                                         "controls",
                                         "controlslist",
                                         "conversiondestination",
                                         "coords",
                                         "crossorigin",
                                         "csp",
                                         "data",
                                         "datetime",
                                         "declare",
                                         "decoding",
                                         "default",
                                         "defer",
                                         "dir",
                                         "direction",
                                         "dirname",
                                         "disabled",
                                         "disablepictureinpicture",
                                         "disableremoteplayback",
                                         "disallowdocumentaccess",
                                         "download",
                                         "draggable",
                                         "elementtiming",
                                         "enctype",
                                         "end",
                                         "enterkeyhint",
                                         "event",
                                         "exportparts",
                                         "face",
                                         "for",
                                         "form",
                                         "formaction",
                                         "formenctype",
                                         "formmethod",
                                         "formnovalidate",
                                         "formtarget",
                                         "frame",
                                         "frameborder",
                                         "headers",
                                         "height",
                                         "hidden",
                                         "high",
                                         "href",
                                         "hreflang",
                                         "hreftranslate",
                                         "hspace",
                                         "http-equiv",
                                         "id",
                                         "imagesizes",
                                         "imagesrcset",
                                         "importance",
                                         "impressiondata",
                                         "impressionexpiry",
                                         "incremental",
                                         "inert",
                                         "inputmode",
                                         "integrity",
                                         "invisible",
                                         "is",
                                         "ismap",
                                         "keytype",
                                         "kind",
                                         "label",
                                         "lang",
                                         "language",
                                         "latencyhint",
                                         "leftmargin",
                                         "link",
                                         "list",
                                         "loading",
                                         "longdesc",
                                         "loop",
                                         "low",
                                         "lowsrc",
                                         "manifest",
                                         "marginheight",
                                         "marginwidth",
                                         "max",
                                         "maxlength",
                                         "mayscript",
                                         "media",
                                         "method",
                                         "min",
                                         "minlength",
                                         "multiple",
                                         "muted",
                                         "name",
                                         "nohref",
                                         "nomodule",
                                         "nonce",
                                         "noresize",
                                         "noshade",
                                         "novalidate",
                                         "nowrap",
                                         "object",
                                         "open",
                                         "optimum",
                                         "part",
                                         "pattern",
                                         "ping",
                                         "placeholder",
                                         "playsinline",
                                         "policy",
                                         "poster",
                                         "preload",
                                         "pseudo",
                                         "readonly",
                                         "referrerpolicy",
                                         "rel",
                                         "reportingorigin",
                                         "required",
                                         "resources",
                                         "rev",
                                         "reversed",
                                         "role",
                                         "placeholder",
                                         "playsinline",
                                         "policy",
                                         "poster",
                                         "preload",
                                         "pseudo",
                                         "readonly",
                                         "referrerpolicy",
                                         "rel",
                                         "reportingorigin",
                                         "required",
                                         "resources",
                                         "rev",
                                         "reversed",
                                         "role",
                                         "rows",
                                         "rowspan",
                                         "rules",
                                         "sandbox",
                                         "scheme",
                                         "scope",
                                         "scopes",
                                         "scrollamount",
                                         "scrolldelay",
                                         "scrolling",
                                         "select",
                                         "selected",
                                         "shadowroot",
                                         "shadowrootdelegatesfocus",
                                         "shape",
                                         "size",
                                         "sizes",
                                         "slot",
                                         "span",
                                         "spellcheck",
                                         "src",
                                         "srcdoc",
                                         "srclang",
                                         "srcset",
                                         "standby",
                                         "start",
                                         "step",
                                         "style",
                                         "summary",
                                         "tabindex",
                                         "target",
                                         "text",
                                         "title",
                                         "topmargin",
                                         "translate",
                                         "truespeed",
                                         "trusttoken",
                                         "type",
                                         "usemap",
                                         "valign",
                                         "value",
                                         "valuetype",
                                         "version",
                                         "virtualkeyboardpolicy",
                                         "vlink",
                                         "vspace",
                                         "webkitdirectory",
                                         "width",
                                         "wrap"};

void ElementFormatter(HashSet<String>& element_set,
                      const Vector<String>& elements) {
  for (const String& s : elements) {
    element_set.insert(s.UpperASCII());
  }
}

void AttrFormatter(HashMap<String, Vector<String>>& attr_map,
                   const Vector<std::pair<String, Vector<String>>>& attrs) {
  Vector<String> kVectorStar = {"*"};
  for (const std::pair<String, Vector<String>>& pair : attrs) {
    const String& lower_attr = pair.first.LowerASCII();
    if (pair.second == kVectorStar || pair.second.Contains("*")) {
      attr_map.insert(lower_attr, kVectorStar);
    } else {
      Vector<String> elements;
      for (const String& s : pair.second) {
        elements.push_back(s.UpperASCII());
      }
      attr_map.insert(lower_attr, elements);
    }
  }
}

SanitizerConfig* BuildDefaultConfig() {
  SanitizerConfig* config = SanitizerConfig::Create();

  Vector<String> allow_elements;
  for (const auto* elem : kDefaultAllowElements)
    allow_elements.push_back(elem);
  config->setAllowElements(allow_elements);

  Vector<String> star = {"*"};
  Vector<std::pair<String, Vector<String>>> allow_attributes;
  for (const auto* attr : kDefaultAllowAttributes)
    allow_attributes.push_back(std::make_pair(attr, star));
  config->setAllowAttributes(allow_attributes);

  config->setAllowCustomElements(false);
  return config;
}

SanitizerConfig* GetDefaultConfig() {
  DEFINE_STATIC_LOCAL(Persistent<SanitizerConfig>, config_,
                      (BuildDefaultConfig()));
  return config_.Get();
}

SanitizerConfigImpl GetDefaultConfigImpl() {
  DEFINE_STATIC_LOCAL(SanitizerConfigImpl, config_,
                      (SanitizerConfigImpl::From(GetDefaultConfig())));
  return config_;
}

}  // anonymous namespace

// Create a SanitizerConfigImpl from a SanitizerConfig.
//
// The SC is a JavaScript dictionary - as defined in IDL and required by the
// spec - which contains all the information, but is not efficiently queryable.
// The SCImpl uses more suitable data structures, but this requires us to
// duplicate the information. This method accompslished this.
SanitizerConfigImpl SanitizerConfigImpl::From(const SanitizerConfig* config) {
  if (!config) {
    return GetDefaultConfigImpl();
  }

  SanitizerConfigImpl impl;

  impl.allow_custom_elements_ =
      config->hasAllowCustomElements() && config->allowCustomElements();

  // Format dropElements to uppercase.
  if (config->hasDropElements()) {
    ElementFormatter(impl.drop_elements_, config->dropElements());
  }

  // Format blockElements to uppercase.
  if (config->hasBlockElements()) {
    ElementFormatter(impl.block_elements_, config->blockElements());
  }

  // Format allowElements to uppercase.
  if (config->hasAllowElements()) {
    ElementFormatter(impl.allow_elements_, config->allowElements());
  } else {
    impl.allow_elements_ = GetDefaultConfigImpl().allow_elements_;
  }

  // Format dropAttributes to lowercase.
  if (config->hasDropAttributes()) {
    AttrFormatter(impl.drop_attributes_, config->dropAttributes());
  }

  // Format allowAttributes to lowercase.
  if (config->hasAllowAttributes()) {
    AttrFormatter(impl.allow_attributes_, config->allowAttributes());
  } else {
    impl.allow_attributes_ = GetDefaultConfigImpl().allow_attributes_;
  }

  return impl;
}

SanitizerConfig* SanitizerConfigImpl::defaultConfig() {
  return GetDefaultConfig();
}

}  // namespace blink
