// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"

#include <algorithm>
#include <iostream>
#include <type_traits>

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

namespace {

template <class Char, size_t n>
bool operator==(base::span<const Char> span, const char (&s)[n]) {
  if (span.size() != n - 1) {
    return false;
  }
  for (size_t i = 0; i < n - 1; ++i) {
    if (span[i] != s[i]) {
      return false;
    }
  }
  return true;
}

template <int n>
constexpr bool OnlyContainsLowercaseASCIILetters(const char (&s)[n]) {
  for (int i = 0; i < n - 1; ++i) {
    if (!('a' <= s[i] && s[i] <= 'z')) {
      return false;
    }
  }
  return true;
}

// A hash function that is just good enough to distinguish the supported
// tagnames. It needs to be adapted as soon as we have colliding tagnames.
// The implementation was chosen to map to a dense integer range to allow for
// compact switch jump-tables. If adding support for a new tag results in a
// collision, then pick a new function that minimizes the number of operations
// and results in a dense integer range. This will require some finesse, feel
// free to reach out to owners of bug 1407201 for help.
template <uint32_t n>
constexpr uint32_t TagnameHash(const char (&s)[n]) {
  // The fast-path parser only scans for letters in tagnames.
  DCHECK(OnlyContainsLowercaseASCIILetters<n>(s));
  DCHECK_EQ('\0', s[n - 1]);
  // This function is called with null-termined string, which should be used in
  // the hash implementation, hence the -2.
  return (s[0] + 17 * s[n - 2]) & 63;
}
template <class Char>
uint32_t TagnameHash(base::span<const Char> s) {
  return (s[0] + 17 * s[s.size() - 1]) & 63;
}
uint32_t TagnameHash(const String& s) {
  uint32_t l = s.length();
  return (s[0] + 17 * s[l - 1]) & 63;
}

#define SUPPORTED_TAGS(V) \
  V(A)                    \
  V(B)                    \
  V(Br)                   \
  V(Button)               \
  V(Div)                  \
  V(Footer)               \
  V(I)                    \
  V(Input)                \
  V(Li)                   \
  V(Label)                \
  V(Option)               \
  V(Ol)                   \
  V(P)                    \
  V(Select)               \
  V(Span)                 \
  V(Strong)               \
  V(Ul)

// This HTML parser is used as a fast-path for setting innerHTML.
// It is faster than the general parser by only supporting a subset of valid
// HTML. This way, it can be spec-compliant without following the algorithm
// described in the spec. Unsupported features or parse errors lead to bailout,
// falling back to the general HTML parser.
// It differs from the general HTML parser in the following ways.
//
// Implementation:
// - It uses recursive descent for better CPU branch prediction.
// - It merges tokenization with parsing.
// - Whenever possible, tokens are represented as subsequences of the original
//   input, avoiding allocating memory for them.
//
// Restrictions (these may evolve based on uma data, https://crbug.com/1407201):
// - No auto-closing of tags.
// - Wrong nesting of HTML elements (for example nested <p>) leads to bailout
//   instead of fix-up.
// - No custom elements, no "is"-attribute.
// - No duplicate attributes. This restriction could be lifted easily.
// - Unquoted attribute names are very restricted.
// - Many tags are unsupported, but we could support more. For example, <table>
//   because of the complex re-parenting rules
// - Only a few named "&" character references are supported.
// - No '\0'. The handling of '\0' varies depending upon where it is found
//   and in general the correct handling complicates things.
// - Fails if an attribute name starts with 'on'. Such attributes are generally
//   events that may be fired. Allowing this could be problematic if the fast
//   path fails. For example, the 'onload' event of an <img> would be called
//   multiple times if parsing fails.
// - Fails if a text is encountered larger than Text::kDefaultLengthLimit. This
//   requires special processing.
// - Fails if a deep hierarchy is encountered. This is both to avoid a crash,
//   but also at a certain depth elements get added as siblings vs children (see
//   use of HTMLConstructionSite::kMaximumHTMLParserDOMTreeDepth).
// - Fails if an <img> is encountered. Image elements request the image early
//   on, resulting in network connections. Additionally, loading the image
//   may consume preloaded resources.
// - Fails if Document::IsDirAttributeDirty() is true and CSSPseudoDirEnabled is
//   enabled. This is necessary as state needed to support css-pseudo dir is set
//   in HTMLElement::BeginParsingChildren(), which this does not call.
template <class Char>
class HTMLFastPathParser {
  STACK_ALLOCATED();
  using Span = base::span<const Char>;
  using USpan = base::span<const UChar>;
  static_assert(std::is_same_v<Char, UChar> || std::is_same_v<Char, LChar>);

 public:
  HTMLFastPathParser(Span source,
                     Document& document,
                     DocumentFragment& fragment)
      : source_(source), document_(document), fragment_(fragment) {}

  bool Run(Element& context_element) {
    QualifiedName context_tag = context_element.TagQName();
    DCHECK(!context_tag.LocalName().empty());

    // This switch checks that the context element is supported and applies the
    // same restrictions regarding content as the fast-path parser does for a
    // corresponding nested tag.
    // This is to ensure that we preserve correct HTML structure with respect
    // to the context tag.
    //
    // If this switch has duplicate cases, then `TagnameHash()` needs to be
    // updated.
    switch (TagnameHash(context_tag.LocalName())) {
#define TAG_CASE(Tagname)                                     \
  case TagnameHash(TagInfo::Tagname::tagname):                \
    DCHECK(html_names::k##Tagname##Tag.LocalName().Ascii() == \
           TagInfo::Tagname::tagname);                        \
    if constexpr (!TagInfo::Tagname::is_void) {               \
      /* The hash function won't return collisions for the */ \
      /* supported tags, but this function takes */           \
      /* potentially unsupported tags, which may collide. */  \
      /* Protect against that by checking equality.  */       \
      if (context_tag == html_names::k##Tagname##Tag) {       \
        ParseCompleteInput<typename TagInfo::Tagname>();      \
        return !failed_;                                      \
      }                                                       \
    }                                                         \
    break;
      SUPPORTED_TAGS(TAG_CASE)
      default:
        break;
#undef TAG_CASE
    }

    Fail(HtmlFastPathResult::kFailedUnsupportedContextTag);
    return false;
  }

  int NumberOfBytesParsed() const {
    return sizeof(Char) * static_cast<int>(pos_ - source_.data());
  }

  HtmlFastPathResult parse_result() const { return parse_result_; }

 private:
  Span source_;
  Document& document_;
  DocumentFragment& fragment_;

  const Char* const end_ = source_.data() + source_.size();
  const Char* pos_ = source_.data();

  bool failed_ = false;
  bool inside_of_tag_a_ = false;
  // Used to limit how deep a hierarchy can be created. Also note that
  // HTMLConstructionSite ends up flattening when this depth is reached.
  unsigned element_depth_ = 0;
  // 32 matches that used by HTMLToken::Attribute.
  Vector<Char, 32> char_buffer_;
  Vector<UChar> uchar_buffer_;
  // Used if the attribute name contains upper case ascii (which must be
  // mapped to lower case).
  // 32 matches that used by HTMLToken::Attribute.
  Vector<Char, 32> attribute_name_buffer_;
  Vector<Attribute, kAttributePrealloc> attribute_buffer_;
  Vector<StringImpl*> attribute_names_;
  HtmlFastPathResult parse_result_ = HtmlFastPathResult::kSucceeded;

  enum class PermittedParents {
    kPhrasingOrFlowContent,  // allowed in phrasing content or flow content
    kFlowContent,  // only allowed in flow content, not in phrasing content
    kSpecial,      // only allowed for special parents
  };

  struct TagInfo {
    template <class T, PermittedParents parents>
    struct Tag {
      using ElemClass = T;
      static constexpr PermittedParents kPermittedParents = parents;
      static ElemClass* Create(Document& document) {
        return MakeGarbageCollected<ElemClass>(document);
      }
      static constexpr bool AllowedInPhrasingOrFlowContent() {
        return kPermittedParents == PermittedParents::kPhrasingOrFlowContent;
      }
      static constexpr bool AllowedInFlowContent() {
        return kPermittedParents == PermittedParents::kPhrasingOrFlowContent ||
               kPermittedParents == PermittedParents::kFlowContent;
      }
    };

    template <class T, PermittedParents parents>
    struct VoidTag : Tag<T, parents> {
      static constexpr bool is_void = true;
    };

    template <class T, PermittedParents parents>
    struct ContainerTag : Tag<T, parents> {
      static constexpr bool is_void = false;

      static Element* ParseChild(HTMLFastPathParser& self) {
        return self.ParseElement</*non_phrasing_content*/ true>();
      }
    };

    // A tag that can only contain phrasing content.
    // If a tag is considered phrasing content itself is decided by
    // `allowed_in_phrasing_content`.
    template <class T, PermittedParents parents>
    struct ContainsPhrasingContentTag : ContainerTag<T, parents> {
      static constexpr bool is_void = false;

      static Element* ParseChild(HTMLFastPathParser& self) {
        return self.ParseElement</*non_phrasing_content*/ false>();
      }
    };

    struct A : ContainerTag<HTMLAnchorElement, PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "a";

      static Element* ParseChild(HTMLFastPathParser& self) {
        DCHECK(!self.inside_of_tag_a_);
        self.inside_of_tag_a_ = true;
        Element* res =
            ContainerTag<HTMLAnchorElement,
                         PermittedParents::kFlowContent>::ParseChild(self);
        self.inside_of_tag_a_ = false;
        return res;
      }
    };

    struct AWithPhrasingContent
        : ContainsPhrasingContentTag<HTMLAnchorElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "a";

      static Element* ParseChild(HTMLFastPathParser& self) {
        DCHECK(!self.inside_of_tag_a_);
        self.inside_of_tag_a_ = true;
        Element* res = ContainsPhrasingContentTag<
            HTMLAnchorElement,
            PermittedParents::kPhrasingOrFlowContent>::ParseChild(self);
        self.inside_of_tag_a_ = false;
        return res;
      }
    };

    struct B
        : ContainsPhrasingContentTag<HTMLElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "b";
      static HTMLElement* Create(Document& document) {
        return MakeGarbageCollected<HTMLElement>(html_names::kBTag, document);
      }
    };

    struct Br
        : VoidTag<HTMLBRElement, PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "br";
    };

    struct Button
        : ContainsPhrasingContentTag<HTMLButtonElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "button";
    };

    struct Div : ContainerTag<HTMLDivElement, PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "div";
    };

    struct Footer
        : ContainerTag<HTMLDivElement, PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "footer";
      static HTMLElement* Create(Document& document) {
        return MakeGarbageCollected<HTMLElement>(html_names::kFooterTag,
                                                 document);
      }
    };

    struct I
        : ContainsPhrasingContentTag<HTMLElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "i";
      static HTMLElement* Create(Document& document) {
        return MakeGarbageCollected<HTMLElement>(html_names::kITag, document);
      }
    };

    struct Input
        : VoidTag<HTMLInputElement, PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "input";
      static HTMLInputElement* Create(Document& document) {
        return MakeGarbageCollected<HTMLInputElement>(
            document, CreateElementFlags::ByFragmentParser(&document));
      }
    };

    struct Li : ContainerTag<HTMLLIElement, PermittedParents::kSpecial> {
      static constexpr const char tagname[] = "li";
    };

    struct Label
        : ContainsPhrasingContentTag<HTMLLabelElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "label";
    };

    struct Option
        : ContainerTag<HTMLOptionElement, PermittedParents::kSpecial> {
      static constexpr const char tagname[] = "option";
      static Element* ParseChild(HTMLFastPathParser& self) {
        // <option> can only contain a text content.
        return self.Fail(HtmlFastPathResult::kFailedOptionWithChild, nullptr);
      }
    };

    struct Ol : ContainerTag<HTMLOListElement, PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "ol";

      static Element* ParseChild(HTMLFastPathParser& self) {
        return self.ParseSpecificElements<Li>();
      }
    };

    struct P : ContainsPhrasingContentTag<HTMLParagraphElement,
                                          PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "p";
    };

    struct Select : ContainerTag<HTMLSelectElement,
                                 PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "select";
      static Element* ParseChild(HTMLFastPathParser& self) {
        return self.ParseSpecificElements<Option>();
      }
    };

    struct Span
        : ContainsPhrasingContentTag<HTMLSpanElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "span";
    };

    struct Strong
        : ContainsPhrasingContentTag<HTMLElement,
                                     PermittedParents::kPhrasingOrFlowContent> {
      static constexpr const char tagname[] = "strong";
      static HTMLElement* Create(Document& document) {
        return MakeGarbageCollected<HTMLElement>(html_names::kStrongTag,
                                                 document);
      }
    };

    struct Ul : ContainerTag<HTMLUListElement, PermittedParents::kFlowContent> {
      static constexpr const char tagname[] = "ul";

      static Element* ParseChild(HTMLFastPathParser& self) {
        return self.ParseSpecificElements<Li>();
      }
    };
  };

  template <class ParentTag>
  void ParseCompleteInput() {
    ParseChildren<ParentTag>(&fragment_);
    if (pos_ != end_) {
      Fail(HtmlFastPathResult::kFailedDidntReachEndOfInput);
    }
  }

  // Match ASCII Whitespace according to
  // https://infra.spec.whatwg.org/#ascii-whitespace
  bool IsWhitespace(Char c) {
    switch (c) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case '\f':
        return true;
      default:
        return false;
    }
  }

  bool IsValidUnquotedAttributeValueChar(Char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') || c == '_' || c == '-';
  }

  // https://html.spec.whatwg.org/#syntax-attribute-name
  bool IsValidAttributeNameChar(Char c) {
    if (c == '=') {
      // Early exit for the most common way to end an attribute.
      return false;
    }
    return ('a' <= c && c <= 'z') || c == '-' || ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9');
  }

  bool IsCharAfterTagnameOrAttribute(Char c) {
    return c == ' ' || c == '>' || IsWhitespace(c) || c == '/';
  }

  bool IsCharAfterUnquotedAttribute(Char c) {
    return c == ' ' || c == '>' || IsWhitespace(c);
  }

  void SkipWhitespace() {
    while (pos_ != end_ && IsWhitespace(*pos_)) {
      ++pos_;
    }
  }

  struct ScanTextResult {
    // HTML strings of the form '\n<space>*' are widespread on the web. Caching
    // them saves us allocations, which improves the runtime.
    String TryCanonicalizeString() const {
      DCHECK(!text.empty());
      if (is_newline_then_whitespace_string &&
          text.size() < WTF::NewlineThenWhitespaceStringsTable::kTableSize) {
#if DCHECK_IS_ON()
        DCHECK(WTF::NewlineThenWhitespaceStringsTable::IsNewlineThenWhitespaces(
            String(text.data(), static_cast<unsigned>(text.size()))));
#endif  // DCHECK_IS_ON()
        return WTF::NewlineThenWhitespaceStringsTable::GetStringForLength(
            text.size());
      }
      return String(text.data(), static_cast<unsigned>(text.size()));
    }

    Span text;
    USpan escaped_text;
    bool is_newline_then_whitespace_string = false;
  };

  // We first try to scan text as an unmodified subsequence of the input.
  // However, if there are escape sequences, we have to copy the text to a
  // separate buffer and we might go outside of `Char` range if we are in an
  // `LChar` parser. Therefore, this function returns either a `Span` or a
  // `USpan`. Callers distinguish the two cases by checking if the `Span` is
  // empty, as only one of them can be non-empty.
  ScanTextResult ScanText() {
    const Char* start = pos_;
    bool is_newline_then_whitespace_string = false;
    if (pos_ != end_ && *pos_ == '\n') {
      is_newline_then_whitespace_string = true;
      ++pos_;
    }
    while (pos_ != end_ && *pos_ != '<') {
      // '&' indicates escape sequences, '\r' might require
      // https://infra.spec.whatwg.org/#normalize-newlines
      if (*pos_ == '&' || *pos_ == '\r') {
        pos_ = start;
        return {Span{}, ScanEscapedText()};
      } else if (UNLIKELY(*pos_ == '\0')) {
        return Fail(HtmlFastPathResult::kFailedContainsNull,
                    ScanTextResult{Span{}, USpan{}});
      }
      if (*pos_ != ' ') {
        is_newline_then_whitespace_string = false;
      }
      ++pos_;
    }
    return {{start, static_cast<size_t>(pos_ - start)},
            USpan{},
            is_newline_then_whitespace_string};
  }

  // Slow-path of `ScanText()`, which supports escape sequences by copying to a
  // separate buffer.
  USpan ScanEscapedText() {
    uchar_buffer_.resize(0);
    while (pos_ != end_ && *pos_ != '<') {
      if (*pos_ == '&') {
        ScanHTMLCharacterReference(&uchar_buffer_);
        if (failed_) {
          return USpan{};
        }
      } else if (*pos_ == '\r') {
        // Normalize "\r\n" to "\n" according to
        // https://infra.spec.whatwg.org/#normalize-newlines.
        if (pos_ + 1 != end_ && pos_[1] == '\n') {
          ++pos_;
        }
        uchar_buffer_.push_back('\n');
        ++pos_;
      } else if (UNLIKELY(*pos_ == '\0')) {
        return Fail(HtmlFastPathResult::kFailedContainsNull, USpan{});
      } else {
        uchar_buffer_.push_back(*pos_);
        ++pos_;
      }
    }
    return {uchar_buffer_.data(), uchar_buffer_.size()};
  }

  // Scan a tagname and convert to lowercase if necessary.
  Span ScanTagname() {
    const Char* start = pos_;
    while (pos_ != end_ && 'a' <= *pos_ && *pos_ <= 'z') {
      ++pos_;
    }
    if (pos_ == end_ || !IsCharAfterTagnameOrAttribute(*pos_)) {
      // Try parsing a case-insensitive tagname.
      char_buffer_.resize(0);
      pos_ = start;
      while (pos_ != end_) {
        Char c = *pos_;
        if ('A' <= c && c <= 'Z') {
          c = c - ('A' - 'a');
        } else if (!('a' <= c && c <= 'z')) {
          break;
        }
        ++pos_;
        char_buffer_.push_back(c);
      }
      if (pos_ == end_ || !IsCharAfterTagnameOrAttribute(*pos_)) {
        return Fail(HtmlFastPathResult::kFailedParsingTagName, Span{});
      }
      SkipWhitespace();
      return Span{char_buffer_.data(), char_buffer_.size()};
    }
    Span res = Span{start, static_cast<size_t>(pos_ - start)};
    SkipWhitespace();
    return res;
  }

  Span ScanAttrName() {
    // First look for all lower case. This path doesn't require any mapping of
    // input. This path could handle other valid attribute name chars, but they
    // are not as common, so it only looks for lowercase.
    const Char* start = pos_;
    while (pos_ != end_ && *pos_ >= 'a' && *pos_ <= 'z') {
      ++pos_;
    }
    if (UNLIKELY(pos_ == end_)) {
      return Fail(HtmlFastPathResult::kFailedEndOfInputReached, Span());
    }
    if (!IsValidAttributeNameChar(*pos_)) {
      return Span(start, static_cast<size_t>(pos_ - start));
    }

    // At this point name does not contain lowercase. It may contain upper-case,
    // which requires mapping. Assume it does.
    pos_ = start;
    attribute_name_buffer_.resize(0);
    Char c;
    // IsValidAttributeNameChar() returns false if end of input is reached.
    while (c = GetNext(), IsValidAttributeNameChar(c)) {
      if ('A' <= c && c <= 'Z') {
        c = c - ('A' - 'a');
      }
      attribute_name_buffer_.push_back(c);
      ++pos_;
    }
    return Span(attribute_name_buffer_.data(),
                static_cast<size_t>(attribute_name_buffer_.size()));
  }

  std::pair<Span, USpan> ScanAttrValue() {
    Span result;
    SkipWhitespace();
    const Char* start = pos_;
    if (Char quote_char = GetNext(); quote_char == '"' || quote_char == '\'') {
      start = ++pos_;
      while (pos_ != end_ && GetNext() != quote_char) {
        if (GetNext() == '&' || GetNext() == '\r') {
          pos_ = start - 1;
          return {Span{}, ScanEscapedAttrValue()};
        }
        ++pos_;
      }
      if (pos_ == end_) {
        return Fail(HtmlFastPathResult::kFailedParsingQuotedAttributeValue,
                    std::pair{Span{}, USpan{}});
      }
      result = Span{start, static_cast<size_t>(pos_ - start)};
      if (ConsumeNext() != quote_char) {
        return Fail(HtmlFastPathResult::kFailedParsingQuotedAttributeValue,
                    std::pair{Span{}, USpan{}});
      }
    } else {
      while (IsValidUnquotedAttributeValueChar(GetNext())) {
        ++pos_;
      }
      result = Span{start, static_cast<size_t>(pos_ - start)};
      if (!IsCharAfterUnquotedAttribute(GetNext())) {
        return Fail(HtmlFastPathResult::kFailedParsingUnquotedAttributeValue,
                    std::pair{Span{}, USpan{}});
      }
    }
    return {result, USpan{}};
  }

  // Slow path for scanning an attribute value. Used for special cases such
  // as '&' and '\r'.
  USpan ScanEscapedAttrValue() {
    Span result;
    SkipWhitespace();
    uchar_buffer_.resize(0);
    const Char* start = pos_;
    if (Char quote_char = GetNext(); quote_char == '"' || quote_char == '\'') {
      start = ++pos_;
      while (pos_ != end_ && GetNext() != quote_char) {
        if (failed_) {
          return USpan{};
        }
        if (GetNext() == '&') {
          ScanHTMLCharacterReference(&uchar_buffer_);
        } else if (GetNext() == '\r') {
          // Normalize "\r\n" to "\n" according to
          // https://infra.spec.whatwg.org/#normalize-newlines.
          if (pos_ + 1 != end_ && pos_[1] == '\n') {
            ++pos_;
          }
          uchar_buffer_.push_back('\n');
          ++pos_;
        } else {
          uchar_buffer_.push_back(*pos_);
          ++pos_;
        }
      }
      if (pos_ == end_) {
        return Fail(
            HtmlFastPathResult::kFailedParsingQuotedEscapedAttributeValue,
            USpan());
      }
      result = Span{start, static_cast<size_t>(pos_ - start)};
      if (ConsumeNext() != quote_char) {
        return Fail(
            HtmlFastPathResult::kFailedParsingQuotedEscapedAttributeValue,
            USpan{});
      }
    } else {
      return Fail(
          HtmlFastPathResult::kFailedParsingUnquotedEscapedAttributeValue,
          USpan{});
    }
    return USpan{uchar_buffer_.data(), uchar_buffer_.size()};
  }

  void ScanHTMLCharacterReference(Vector<UChar>* out) {
    DCHECK_EQ(*pos_, '&');
    ++pos_;
    const Char* start = pos_;
    while (true) {
      // A rather arbitrary constant to prevent unbounded lookahead in the case
      // of ill-formed input.
      constexpr int kMaxLength = 20;
      if (pos_ == end_ || pos_ - start > kMaxLength ||
          UNLIKELY(*pos_ == '\0')) {
        return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
      }
      if (ConsumeNext() == ';') {
        break;
      }
    }
    Span reference = Span{start, static_cast<size_t>(pos_ - start) - 1};
    // There are no valid character references shorter than that. The check
    // protects the indexed accesses below.
    constexpr size_t kMinLength = 2;
    if (reference.size() < kMinLength) {
      return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
    }
    if (reference[0] == '#') {
      UChar32 res = 0;
      if (reference[1] == 'x' || reference[1] == 'X') {
        for (size_t i = 2; i < reference.size(); ++i) {
          Char c = reference[i];
          res *= 16;
          if (c >= '0' && c <= '9') {
            res += c - '0';
          } else if (c >= 'a' && c <= 'f') {
            res += c - 'a' + 10;
          } else if (c >= 'A' && c <= 'F') {
            res += c - 'A' + 10;
          } else {
            return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
          }
          if (res > UCHAR_MAX_VALUE) {
            return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
          }
        }
      } else {
        for (size_t i = 1; i < reference.size(); ++i) {
          Char c = reference[i];
          res *= 10;
          if (c >= '0' && c <= '9') {
            res += c - '0';
          } else {
            return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
          }
          if (res > UCHAR_MAX_VALUE) {
            return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
          }
        }
      }
      DecodedHTMLEntity entity;
      AppendLegalEntityFor(res, entity);
      for (size_t i = 0; i < entity.length; ++i) {
        out->push_back(entity.data[i]);
      }
      // Handle the most common named references.
    } else if (reference == "amp") {
      out->push_back('&');
    } else if (reference == "lt") {
      out->push_back('<');
    } else if (reference == "gt") {
      out->push_back('>');
    } else if (reference == "nbsp") {
      out->push_back(0xa0);
    } else {
      // This handles uncommon named references.
      String input_string{reference.data(),
                          static_cast<unsigned>(reference.size())};
      SegmentedString input_segmented{input_string};
      DecodedHTMLEntity entity;
      bool not_enough_characters = false;
      if (!ConsumeHTMLEntity(input_segmented, entity, not_enough_characters) ||
          not_enough_characters) {
        return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
      }
      for (size_t i = 0; i < entity.length; ++i) {
        out->push_back(entity.data[i]);
      }
    }
  }

  void Fail(HtmlFastPathResult result) {
    // This function may be called multiple times. Only record the result the
    // first time it's called.
    if (failed_) {
      return;
    }
    parse_result_ = result;
    failed_ = true;
  }

  template <class R>
  R Fail(HtmlFastPathResult result, R res) {
    Fail(result);
    return res;
  }

  Char GetNext() {
    DCHECK_LE(pos_, end_);
    if (pos_ == end_) {
      Fail(HtmlFastPathResult::kFailedEndOfInputReached);
      return '\0';
    }
    return *pos_;
  }

  Char ConsumeNext() {
    if (pos_ == end_) {
      return Fail(HtmlFastPathResult::kFailedEndOfInputReached, '\0');
    }
    return *(pos_++);
  }

  template <class ParentTag>
  void ParseChildren(ContainerNode* parent) {
    while (true) {
      ScanTextResult scanned_text = ScanText();
      if (failed_) {
        return;
      }
      DCHECK(scanned_text.text.empty() || scanned_text.escaped_text.empty());
      if (!scanned_text.text.empty()) {
        const auto text = scanned_text.text;
        if (text.size() >= Text::kDefaultLengthLimit) {
          return Fail(HtmlFastPathResult::kFailedBigText);
        }
        parent->ParserAppendChild(
            Text::Create(document_, scanned_text.TryCanonicalizeString()));
      } else if (!scanned_text.escaped_text.empty()) {
        if (scanned_text.escaped_text.size() >= Text::kDefaultLengthLimit) {
          return Fail(HtmlFastPathResult::kFailedBigText);
        }
        parent->ParserAppendChild(Text::Create(
            document_,
            String(scanned_text.escaped_text.data(),
                   static_cast<unsigned>(scanned_text.escaped_text.size()))));
      }
      if (pos_ == end_) {
        return;
      }
      DCHECK_EQ(*pos_, '<');
      ++pos_;
      if (GetNext() == '/') {
        // We assume that we found the closing tag. The tagname will be checked
        // by the caller `ParseContainerElement()`.
        return;
      } else {
        if (++element_depth_ ==
            HTMLConstructionSite::kMaximumHTMLParserDOMTreeDepth) {
          return Fail(HtmlFastPathResult::kFailedMaxDepth);
        }
        Element* child = ParentTag::ParseChild(*this);
        --element_depth_;
        if (failed_) {
          return;
        }
        parent->ParserAppendChild(child);
      }
    }
  }

  Attribute ProcessAttribute(Span name_span,
                             std::pair<Span, USpan> value_span) {
    QualifiedName name = LookupHTMLAttributeName(
        name_span.data(), static_cast<unsigned>(name_span.size()));
    if (name == g_null_name) {
      name =
          QualifiedName(g_null_atom,
                        AtomicString(name_span.data(),
                                     static_cast<unsigned>(name_span.size())),
                        g_null_atom);
    }

    // The string pointer in |value| is null for attributes with no values, but
    // the null atom is used to represent absence of attributes; attributes with
    // no values have the value set to an empty atom instead.
    AtomicString value;
    if (value_span.second.empty()) {
      value = AtomicString(value_span.first.data(),
                           static_cast<unsigned>(value_span.first.size()));
    } else {
      value = AtomicString(value_span.second.data(),
                           static_cast<unsigned>(value_span.second.size()));
    }
    if (value.IsNull()) {
      value = g_empty_atom;
    }
    return Attribute(std::move(name), std::move(value));
  }

  void ParseAttributes(Element* parent) {
    DCHECK(attribute_buffer_.empty());
    DCHECK(attribute_names_.empty());
    while (true) {
      Span attr_name = ScanAttrName();
      if (attr_name.empty()) {
        if (GetNext() == '>') {
          ++pos_;
          break;
        } else if (GetNext() == '/') {
          ++pos_;
          SkipWhitespace();
          if (ConsumeNext() != '>') {
            return Fail(HtmlFastPathResult::kFailedParsingAttributes);
          }
          break;
        } else {
          return Fail(HtmlFastPathResult::kFailedParsingAttributes);
        }
      }
      if (attr_name.size() >= 2 && attr_name[0] == 'o' && attr_name[1] == 'n') {
        // These attributes likely contain script that may be executed at random
        // points, which could cause problems if parsing via the fast path
        // fails. For example, an image's onload event.
        return Fail(HtmlFastPathResult::kFailedOnAttribute);
      }
      SkipWhitespace();
      std::pair<Span, USpan> attr_value = {};
      if (GetNext() == '=') {
        ++pos_;
        attr_value = ScanAttrValue();
        SkipWhitespace();
      }
      Attribute attribute = ProcessAttribute(attr_name, attr_value);
      attribute_buffer_.push_back(attribute);
      if (attribute.GetName() == html_names::kIsAttr) {
        return Fail(HtmlFastPathResult::kFailedParsingAttributes);
      }
      attribute_names_.push_back(attribute.LocalName().Impl());
    }
    std::sort(attribute_names_.begin(), attribute_names_.end());
    if (std::adjacent_find(attribute_names_.begin(), attribute_names_.end()) !=
        attribute_names_.end()) {
      // Found duplicate attributes. We would have to ignore repeated
      // attributes, but leave this to the general parser instead.
      return Fail(HtmlFastPathResult::kFailedParsingAttributes);
    }
    parent->ParserSetAttributes(attribute_buffer_);
    attribute_buffer_.clear();
    attribute_names_.resize(0);
  }

  template <class... Tags>
  Element* ParseSpecificElements() {
    Span tagname = ScanTagname();
    return ParseSpecificElements<Tags...>(tagname);
  }

  template <void* = nullptr>
  Element* ParseSpecificElements(Span tagname) {
    return Fail(HtmlFastPathResult::kFailedParsingSpecificElements, nullptr);
  }

  template <class Tag, class... OtherTags>
  Element* ParseSpecificElements(Span tagname) {
    if (tagname == Tag::tagname) {
      return ParseElementAfterTagname<Tag>();
    }
    return ParseSpecificElements<OtherTags...>(tagname);
  }

  template <bool non_phrasing_content>
  Element* ParseElement() {
    Span tagname = ScanTagname();
    if (tagname.empty()) {
      return Fail(HtmlFastPathResult::kFailedParsingElement, nullptr);
    }
    // HTML has complicated rules around auto-closing tags and re-parenting
    // DOM nodes. We avoid complications with auto-closing rules by disallowing
    // certain nesting. In particular, we bail out if non-phrasing-content
    // elements are nested into elements that require phrasing content.
    // Similarly, we disallow nesting <a> tags. But tables for example have
    // complex re-parenting rules that cannot be captured in this way, so we
    // cannot support them.
    //
    // If this switch has duplicate cases, then `TagnameHash()` needs to be
    // updated.
    switch (TagnameHash(tagname)) {
#define TAG_CASE(Tagname)                                                     \
  case TagnameHash(TagInfo::Tagname::tagname):                                \
    if (std::is_same_v<typename TagInfo::A, typename TagInfo::Tagname>) {     \
      goto case_a;                                                            \
    }                                                                         \
    if constexpr (non_phrasing_content                                        \
                      ? TagInfo::Tagname::AllowedInFlowContent()              \
                      : TagInfo::Tagname::AllowedInPhrasingOrFlowContent()) { \
      /* See comment in Run() for details on why equality is checked */       \
      /* here. */                                                             \
      if (tagname == TagInfo::Tagname::tagname) {                             \
        return ParseElementAfterTagname<typename TagInfo::Tagname>();         \
      }                                                                       \
    }                                                                         \
    break;

      SUPPORTED_TAGS(TAG_CASE)
#undef TAG_CASE

    case_a:
      // <a> tags must not be nested, because HTML parsing would auto-close
      // the outer one when encountering a nested one.
      if (tagname == TagInfo::A::tagname && !inside_of_tag_a_) {
        return non_phrasing_content
                   ? ParseElementAfterTagname<typename TagInfo::A>()
                   : ParseElementAfterTagname<
                         typename TagInfo::AWithPhrasingContent>();
      }
      break;
      default:
        break;
    }
    return Fail(HtmlFastPathResult::kFailedUnsupportedTag, nullptr);
  }

  template <class Tag>
  Element* ParseElementAfterTagname() {
    if constexpr (Tag::is_void) {
      return ParseVoidElement(Tag::Create(document_));
    } else {
      return ParseContainerElement<Tag>(Tag::Create(document_));
    }
  }

  template <class Tag>
  Element* ParseContainerElement(Element* element) {
    ParseAttributes(element);
    if (failed_) {
      return element;
    }
    ParseChildren<Tag>(element);
    if (failed_ || pos_ == end_) {
      return Fail(HtmlFastPathResult::kFailedEndOfInputReachedForContainer,
                  element);
    }
    // ParseChildren<Tag>(element) stops after the (hopefully) closing tag's `<`
    // and fails if the the current char is not '/'.
    DCHECK_EQ(*pos_, '/');
    ++pos_;
    Span endtag = ScanTagname();
    if (endtag == Tag::tagname) {
      if (ConsumeNext() != '>') {
        return Fail(HtmlFastPathResult::kFailedUnexpectedTagNameCloseState,
                    element);
      }
    } else {
      return Fail(HtmlFastPathResult::kFailedEndTagNameMismatch, element);
    }
    return element;
  }

  Element* ParseVoidElement(Element* element) {
    ParseAttributes(element);
    return element;
  }
};

void LogFastPathResult(HtmlFastPathResult result) {
  base::UmaHistogramEnumeration("Blink.HTMLFastPathParser.ParseResult", result);
  if (result != HtmlFastPathResult::kSucceeded) {
    VLOG(2) << "innerHTML fast-path parser failed, "
            << static_cast<int>(result);
  }
}

bool CanUseFastPath(Document& document,
                    Element& context_element,
                    ParserContentPolicy policy,
                    bool include_shadow_roots) {
  if (include_shadow_roots) {
    LogFastPathResult(HtmlFastPathResult::kFailedShadowRoots);
    return false;
  }

  // Disable when tracing is enabled to preserve trace behavior.
  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("devtools.timeline", &tracing_enabled);
  if (tracing_enabled) {
    LogFastPathResult(HtmlFastPathResult::kFailedTracingEnabled);
    return false;
  }

  // We could probably allow other content policies too, as we do not support
  // scripts or plugins anyway.
  if (policy != ParserContentPolicy::kAllowScriptingContent) {
    LogFastPathResult(HtmlFastPathResult::kFailedParserContentPolicy);
    return false;
  }
  // If we are within a form element, we would need to create associations,
  // which we do not. Therefore, we do not support this case.
  // See HTMLConstructionSite::InitFragmentParsing() and
  // HTMLConstructionSite::CreateElement() for the corresponding code on the
  // slow-path.
  if (!context_element.GetDocument().IsTemplateDocument() &&
      Traversal<HTMLFormElement>::FirstAncestorOrSelf(context_element) !=
          nullptr) {
    LogFastPathResult(HtmlFastPathResult::kFailedInForm);
    return false;
  }

  if (document.IsDirAttributeDirty() &&
      RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
    LogFastPathResult(
        HtmlFastPathResult::kFailedCssPseudoDirEnabledAndDirAttributeDirty);
    return false;
  }
  return true;
}

template <class Char>
bool TryParsingHTMLFragmentImpl(const base::span<const Char>& source,
                                Document& document,
                                DocumentFragment& fragment,
                                Element& context_element) {
  base::ElapsedTimer parse_timer;
  bool success;
  int number_of_bytes_parsed;
  HTMLFastPathParser<Char> parser{source, document, fragment};
  success = parser.Run(context_element);
  // The direction attribute may change as a result of parsing. Check again.
  if (document.IsDirAttributeDirty() &&
      RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
    LogFastPathResult(
        HtmlFastPathResult::kFailedCssPseudoDirEnabledAndDirAttributeDirty);
    success = false;
  } else {
    LogFastPathResult(parser.parse_result());
  }
  number_of_bytes_parsed = parser.NumberOfBytesParsed();
  // The time needed to parse is typically < 1ms (even at the 99%).
  if (base::TimeTicks::IsHighResolution()) {
    if (success) {
      base::UmaHistogramCustomMicrosecondsTimes(
          "Blink.HTMLFastPathParser.SuccessfulParseTime2",
          parse_timer.Elapsed(), base::Microseconds(1), base::Milliseconds(10),
          100);
    } else {
      base::UmaHistogramCustomMicrosecondsTimes(
          "Blink.HTMLFastPathParser.AbortedParseTime2", parse_timer.Elapsed(),
          base::Microseconds(1), base::Milliseconds(10), 100);
    }
  }
  base::UmaHistogramCounts10M(
      success ? "Blink.HTMLFastPathParser.SuccessfulParseSize"
              : "Blink.HTMLFastPathParser.AbortedParseSize",
      number_of_bytes_parsed);
  return success;
}

}  // namespace

bool TryParsingHTMLFragment(const String& source,
                            Document& document,
                            DocumentFragment& fragment,
                            Element& context_element,
                            ParserContentPolicy policy,
                            bool include_shadow_roots) {
  if (!CanUseFastPath(document, context_element, policy,
                      include_shadow_roots)) {
    return false;
  }
  return source.Is8Bit()
             ? TryParsingHTMLFragmentImpl<LChar>(source.Span8(), document,
                                                 fragment, context_element)
             : TryParsingHTMLFragmentImpl<UChar>(source.Span16(), document,
                                                 fragment, context_element);
}

#undef SUPPORTED_TAGS

}  // namespace blink
