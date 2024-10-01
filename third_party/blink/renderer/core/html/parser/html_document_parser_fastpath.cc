// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

#if defined(BLINK_ENABLE_VECTORIZED_HTML_SCANNING)
#include "third_party/highway/src/hwy/highway.h"
#define VECTORIZE_SCANNING 1
#else
#define VECTORIZE_SCANNING 0
#endif

namespace blink {

namespace {

#if VECTORIZE_SCANNING
// We use the vectorized classification trick to scan and classify characters.
// Instead of checking the string byte-by-byte (or vector-by-vector), the
// algorithm takes the lower nibbles (4-bits) of the passed string and uses them
// as offsets into the table, represented as a vector. The values corresponding
// to those offsets are actual interesting symbols. The algorithm then simply
// compares the looked up values with the input vector. The true lanes in the
// resulting vector correspond to the interesting symbols.
//
// A big shout out to Daniel Lemire for suggesting the idea. See more on
// vectorized classification in the Daniel's paper:
// https://arxiv.org/pdf/1902.08318.
//
// For relatively short incoming strings (less than 64 characters) it's assumed
// that byte-by-byte comparison is faster. TODO(340582182): According to
// microbenchmarks on M1, string larger than 16 bytes are already scanned faster
// with SIMD.
constexpr size_t kVectorizationThreshold = 64;
// The byte that shall never match any symbol. Using 0xff for it is okay since
// we only want to match ASCII chars (<=128).
constexpr uint8_t kNeverMatchedChar = 0xff;

// The result of the TryMatch function (see below). Contains the index inside
// the vector (the lane) and the found character.
struct MatchedCharacter {
  bool Matched() const { return found_character != kNeverMatchedChar; }

  size_t index_in_vector = 0;
  uint8_t found_character = kNeverMatchedChar;
};

// Tries to match the characters for the single vector. If matched, returns the
// first matched character in the vector.
template <typename D, typename VectorT>
  requires(sizeof(hwy::HWY_NAMESPACE::TFromD<D>) == 1)
HWY_ATTR ALWAYS_INLINE MatchedCharacter TryMatch(D tag,
                                                 VectorT input,
                                                 VectorT low_nibble_table,
                                                 VectorT low_nib_and_mask) {
  namespace hw = hwy::HWY_NAMESPACE;

  // Get the low nibbles.
  const auto nib_lo = input & low_nib_and_mask;
  // Lookup the values in the table using the nibbles as offsets into the table.
  const auto shuf_lo = hw::TableLookupBytes(low_nibble_table, nib_lo);
  // The values in the tables correspond to the interesting symbols. Just
  // compare them with the input vector.
  const auto result = shuf_lo == input;
  // Find the interesting symbol.
  if (const intptr_t index = hw::FindFirstTrue(tag, result); index != -1) {
    return {static_cast<size_t>(index), hw::ExtractLane(input, index)};
  }

  return {};
}

// Scans the 1-byte string and returns the first matched character (1-byte) or
// kNeverMatchedChar otherwise.
template <typename T, typename VectorT>
  requires(sizeof(T) == 1)
HWY_ATTR ALWAYS_INLINE uint8_t SimdAdvanceAndLookup(const T*& start,
                                                    const T* end,
                                                    VectorT low_nibble_table) {
  namespace hw = hwy::HWY_NAMESPACE;
  DCHECK_GE(static_cast<size_t>(end - start), kVectorizationThreshold);

  hw::FixedTag<uint8_t, 16> tag;
  static constexpr auto stride = hw::MaxLanes(tag);

  const auto low_nib_and_mask = hw::Set(tag, 0xf);

  // The main scanning loop.
  for (; start + (stride - 1) < end; start += stride) {
    const auto input = hw::LoadU(tag, reinterpret_cast<const uint8_t*>(start));
    if (const auto result =
            TryMatch(tag, input, low_nibble_table, low_nib_and_mask);
        result.Matched()) {
      start = reinterpret_cast<const T*>(start + result.index_in_vector);
      return result.found_character;
    };
  }

  // Scan the last stride.
  if (start < end) {
    const auto input =
        hw::LoadU(tag, reinterpret_cast<const uint8_t*>(end - stride));
    if (const auto result =
            TryMatch(tag, input, low_nibble_table, low_nib_and_mask);
        result.Matched()) {
      start = end - stride + result.index_in_vector;
      return result.found_character;
    }
    start = end;
  }

  return kNeverMatchedChar;
}

// This overload for 2-bytes strings uses the interleaved load to check the
// lower bytes of the string. We don't use the gather instruction, since it's
// not available on NEON (as opposed to SVE) and is emulated in Highway.
template <typename T, typename VectorT>
  requires(sizeof(T) == 2)
HWY_ATTR ALWAYS_INLINE uint8_t SimdAdvanceAndLookup(const T*& start,
                                                    const T* end,
                                                    VectorT low_nibble_table) {
  namespace hw = hwy::HWY_NAMESPACE;
  DCHECK_GE(static_cast<size_t>(end - start), kVectorizationThreshold);

  hw::FixedTag<uint8_t, 16> tag;
  static constexpr auto stride = hw::MaxLanes(tag);

  const auto low_nib_and_mask = hw::Set(tag, 0xf);

  // The main scanning loop.
  while (start + (stride - 1) < end) {
    VectorT dummy_upper;
    VectorT input;
    hw::LoadInterleaved2(tag, reinterpret_cast<const uint8_t*>(start), input,
                         dummy_upper);
    if (const auto result =
            TryMatch(tag, input, low_nibble_table, low_nib_and_mask);
        result.Matched()) {
      const auto index = result.index_in_vector;
      // Check if the upper byte is zero.
      if (*(start + index) >> 8 == 0) {
        start = reinterpret_cast<const T*>(start + index);
        return result.found_character;
      }

      start += index + 1;
      continue;
    }

    // Otherwise, continue scanning.
    start += stride;
  }

  // Scan the last stride.
  if (start < end) {
    VectorT dummy_upper;
    VectorT input;
    hw::LoadInterleaved2(tag, reinterpret_cast<const uint8_t*>(end - stride),
                         input, dummy_upper);
    for (auto result = TryMatch(tag, input, low_nibble_table, low_nib_and_mask);
         result.Matched();
         result = TryMatch(tag, input, low_nibble_table, low_nib_and_mask)) {
      const auto index = result.index_in_vector;
      // Check if the upper byte is zero.
      if (*(end - stride + index) >> 8 == 0) {
        start = reinterpret_cast<const T*>(end - stride + index);
        return result.found_character;
      }

      // Otherwise, set the corresponding lane to kNeverMatchedChar to never
      // match it again and continue.
      input = hw::InsertLane(input, index, kNeverMatchedChar);
    }
    start = end;
  }

  return kNeverMatchedChar;
}
#endif  // VECTORIZE_SCANNING

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

template <class Char, size_t n>
bool SpanMatchesLowercase(base::span<const Char> span, const char (&s)[n]) {
  DCHECK_EQ(span.size(), n - 1);
  for (size_t i = 0; i < n - 1; ++i) {
    Char lower =
        (span[i] >= 'A' && span[i] <= 'Z') ? span[i] - 'A' + 'a' : span[i];
    if (lower != s[i]) {
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

using UCharLiteralBufferType = UCharLiteralBuffer<32>;

template <class Char>
struct ScanTextResult {
  // Converts `text` to a String. This handles converting UChar to LChar if
  // possible.
  String TextToString() const;

  // HTML strings of the form '\n<space>*' are widespread on the web. Caching
  // them saves us allocations, which improves the runtime.
  String TryCanonicalizeString() const {
    DCHECK(!text.empty());
    if (is_newline_then_whitespace_string &&
        text.size() < WTF::NewlineThenWhitespaceStringsTable::kTableSize) {
      DCHECK(WTF::NewlineThenWhitespaceStringsTable::IsNewlineThenWhitespaces(
          String(text)));
      return WTF::NewlineThenWhitespaceStringsTable::GetStringForLength(
          text.size());
    }
    return TextToString();
  }

  base::span<const Char> text;
  UCharLiteralBufferType* escaped_text = nullptr;
  bool is_newline_then_whitespace_string = false;
};

template <>
String ScanTextResult<LChar>::TextToString() const {
  return String(text);
}

template <>
String ScanTextResult<UChar>::TextToString() const {
  return String(StringImpl::Create8BitIfPossible(text));
}

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
template <class Char>
class HTMLFastPathParser {
  STACK_ALLOCATED();
  using Span = base::span<const Char>;
  using USpan = base::span<const UChar>;
  // 32 matches that used by HTMLToken::Attribute.
  typedef std::conditional<std::is_same_v<Char, UChar>,
                           UCharLiteralBuffer<32>,
                           LCharLiteralBuffer<32>>::type LiteralBufferType;
  static_assert(std::is_same_v<Char, UChar> || std::is_same_v<Char, LChar>);

 public:
  HTMLFastPathParser(Span source, Document& document, ContainerNode& root_node)
      : source_(source), document_(document), root_node_(root_node) {}

  bool Run(Element& context_element, HTMLFragmentParsingBehaviorSet behavior) {
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
      case TagnameHash(TagInfo::Body::tagname):
        if (context_tag == html_names::kBodyTag) {
          if (behavior.Has(HTMLFragmentParsingBehavior::
                               kStripInitialWhitespaceForBody)) {
            SkipWhitespace();
          }
          ParseCompleteInput<typename TagInfo::Body>();
          return !failed_;
        }
        break;
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
  ContainerNode& root_node_;

  const Char* const end_ = source_.data() + source_.size();
  const Char* pos_ = source_.data();

  bool failed_ = false;
  bool inside_of_tag_a_ = false;
  bool inside_of_tag_li_ = false;
  // Used to limit how deep a hierarchy can be created. Also note that
  // HTMLConstructionSite ends up flattening when this depth is reached.
  unsigned element_depth_ = 0;
  LiteralBufferType char_buffer_;
  UCharLiteralBufferType uchar_buffer_;
  // Used if the attribute name contains upper case ascii (which must be
  // mapped to lower case).
  LiteralBufferType attribute_name_buffer_;
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

    struct Body : ContainerTag<HTMLBodyElement, PermittedParents::kSpecial> {
      static constexpr const char tagname[] = "body";
      static HTMLElement* Create(Document& document) {
        // Body is only supported as an element for adding children, and not
        // a node that is created by this code.
        CHECK(false);
        return nullptr;
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

    struct Footer : ContainerTag<HTMLElement, PermittedParents::kFlowContent> {
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

    struct Li : ContainerTag<HTMLLIElement, PermittedParents::kFlowContent> {
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
    ParseChildren<ParentTag>(&root_node_);
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

#if VECTORIZE_SCANNING
  ALWAYS_INLINE HWY_ATTR ScanTextResult<Char> ScanTextVectorized(
      const Char* initial_start) {
    namespace hw = hwy::HWY_NAMESPACE;
    DCHECK_GE(static_cast<size_t>(end_ - pos_), kVectorizationThreshold);
    hw::FixedTag<uint8_t, 16> tag;
    // ASCII representation of interesting symbols:
    //   <: 0011 1100
    //  \r: 0000 1101
    //  \0: 0000 0000
    //   &: 0010 0110
    // The lower nibbles represent offsets into the |low_nibble_table|. The
    // values in the table are the corresponding characters.
    const auto low_nibble_table = hw::Dup128VecFromValues(
        tag, '\0', 0, 0, 0, 0, 0, '&', 0, 0, 0, 0, 0, '<', '\r', 0, 0);
    switch (SimdAdvanceAndLookup(pos_, end_, low_nibble_table)) {
      case kNeverMatchedChar:
        DCHECK_EQ(pos_, end_);
        return {{initial_start, static_cast<size_t>(pos_ - initial_start)},
                nullptr};
      case '\0':
        DCHECK_EQ(*pos_, '\0');
        return Fail(HtmlFastPathResult::kFailedContainsNull,
                    ScanTextResult<Char>{Span{}, nullptr});
      case '<':
        DCHECK_EQ(*pos_, '<');
        return {{initial_start, static_cast<size_t>(pos_ - initial_start)},
                nullptr};
      case '&':
      case '\r':
        DCHECK(*pos_ == '&' || *pos_ == '\r');
        pos_ = initial_start;
        return {Span{}, ScanEscapedText()};
    };

    NOTREACHED();
    return {};
  }
#endif  // VECTORIZE_SCANNING

  // We first try to scan text as an unmodified subsequence of the input.
  // However, if there are escape sequences, we have to copy the text to a
  // separate buffer and we might go outside of `Char` range if we are in an
  // `LChar` parser. Therefore, this function returns either a `Span` or a
  // `USpan`. Callers distinguish the two cases by checking if the `Span` is
  // empty, as only one of them can be non-empty.
  ScanTextResult<Char> ScanText() {
    const Char* start = pos_;

    // First, try to check if the test is a canonical whitespace string.
    if (pos_ != end_ && *pos_ == '\n') {
      while (++pos_ != end_ && *pos_ == ' ')
        ;
      if (pos_ == end_ || *pos_ == '<') {
        return {{start, static_cast<size_t>(pos_ - start)},
                nullptr,
                /*is_newline_then_whitespace_string=*/true};
      }
    }

#if VECTORIZE_SCANNING
    if (static_cast<size_t>(end_ - pos_) >= kVectorizationThreshold) {
      return ScanTextVectorized(start);
    }
#endif  // VECTORIZE_SCANNING

    while (pos_ != end_ && *pos_ != '<') {
      // '&' indicates escape sequences, '\r' might require
      // https://infra.spec.whatwg.org/#normalize-newlines
      if (*pos_ == '&' || *pos_ == '\r') {
        pos_ = start;
        return {Span{}, ScanEscapedText()};
      } else if (*pos_ == '\0') [[unlikely]] {
        return Fail(HtmlFastPathResult::kFailedContainsNull,
                    ScanTextResult<Char>{Span{}, nullptr});
      }
      ++pos_;
    }

    return {{start, static_cast<size_t>(pos_ - start)}, nullptr};
  }

  // Slow-path of `ScanText()`, which supports escape sequences by copying to a
  // separate buffer.
  UCharLiteralBufferType* ScanEscapedText() {
    uchar_buffer_.clear();
    while (pos_ != end_ && *pos_ != '<') {
      if (*pos_ == '&') {
        ScanHTMLCharacterReference(&uchar_buffer_);
        if (failed_) {
          return nullptr;
        }
      } else if (*pos_ == '\r') {
        // Normalize "\r\n" to "\n" according to
        // https://infra.spec.whatwg.org/#normalize-newlines.
        if (pos_ + 1 != end_ && pos_[1] == '\n') {
          ++pos_;
        }
        uchar_buffer_.AddChar('\n');
        ++pos_;
      } else if (*pos_ == '\0') [[unlikely]] {
        return Fail(HtmlFastPathResult::kFailedContainsNull, nullptr);
      } else {
        uchar_buffer_.AddChar(*pos_);
        ++pos_;
      }
    }
    return &uchar_buffer_;
  }

  // Scan a tagname and convert to lowercase if necessary.
  Span ScanTagname() {
    const Char* start = pos_;
    while (pos_ != end_ && 'a' <= *pos_ && *pos_ <= 'z') {
      ++pos_;
    }
    if (pos_ == end_ || !IsCharAfterTagnameOrAttribute(*pos_)) {
      // Try parsing a case-insensitive tagname.
      char_buffer_.clear();
      pos_ = start;
      while (pos_ != end_) {
        Char c = *pos_;
        if ('A' <= c && c <= 'Z') {
          c = c - ('A' - 'a');
        } else if (!('a' <= c && c <= 'z')) {
          break;
        }
        ++pos_;
        char_buffer_.AddChar(c);
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
    while (pos_ != end_ && ((*pos_ >= 'a' && *pos_ <= 'z') || *pos_ == '-')) {
      ++pos_;
    }
    if (pos_ == end_) [[unlikely]] {
      return Fail(HtmlFastPathResult::kFailedEndOfInputReached, Span());
    }
    if (!IsValidAttributeNameChar(*pos_)) {
      return Span(start, static_cast<size_t>(pos_ - start));
    }

    // At this point name does not contain lowercase. It may contain upper-case,
    // which requires mapping. Assume it does.
    pos_ = start;
    attribute_name_buffer_.clear();
    Char c;
    // IsValidAttributeNameChar() returns false if end of input is reached.
    while (c = GetNext(), IsValidAttributeNameChar(c)) {
      if ('A' <= c && c <= 'Z') {
        c = c - ('A' - 'a');
      }
      attribute_name_buffer_.AddChar(c);
      ++pos_;
    }
    return Span(attribute_name_buffer_.data(),
                static_cast<size_t>(attribute_name_buffer_.size()));
  }

#if VECTORIZE_SCANNING
  ALWAYS_INLINE uint8_t
  ScanAttrValueVectorizedWithSingleQuote(const Char* initial_start) {
    namespace hw = hwy::HWY_NAMESPACE;
    DCHECK_GE(static_cast<size_t>(end_ - pos_), kVectorizationThreshold);
    hw::FixedTag<uint8_t, 16> tag;
    // ASCII representation of interesting symbols:
    //   ': 0010 0111
    //  \r: 0000 1101
    //  \0: 0000 0000
    //   &: 0010 0110
    // The lower nibbles represent offsets into the |low_nibble_table|. The
    // values in the table are the corresponding characters.
    const auto low_nibble_table = hw::Dup128VecFromValues(
        tag, '\0', 0, 0, 0, 0, 0, '&', '\'', 0, 0, 0, 0, 0, '\r', 0, 0);
    return SimdAdvanceAndLookup(pos_, end_, low_nibble_table);
  }

  ALWAYS_INLINE uint8_t
  ScanAttrValueVectorizedWithDoubleQuote(const Char* initial_start) {
    namespace hw = hwy::HWY_NAMESPACE;
    DCHECK_GE(static_cast<size_t>(end_ - pos_), kVectorizationThreshold);
    hw::FixedTag<uint8_t, 16> tag;
    // ASCII representation of interesting symbols:
    //   ": 0010 0010
    //  \r: 0000 1101
    //  \0: 0000 0000
    //   &: 0010 0110
    // The lower nibbles represent offsets into the |low_nibble_table|. The
    // values in the table are the corresponding characters.
    const auto low_nibble_table = hw::Dup128VecFromValues(
        tag, '\0', 0, '"', 0, 0, 0, '&', 0, 0, 0, 0, 0, 0, '\r', 0, 0);
    return SimdAdvanceAndLookup(pos_, end_, low_nibble_table);
  }

  ALWAYS_INLINE std::pair<Span, USpan> ScanAttrValueVectorized(
      Char quote_symbol,
      const Char* initial_start) {
    DCHECK(quote_symbol == '\'' || quote_symbol == '\"');
    const uint8_t found_character =
        quote_symbol == '\''
            ? ScanAttrValueVectorizedWithSingleQuote(initial_start)
            : ScanAttrValueVectorizedWithDoubleQuote(initial_start);

    switch (found_character) {
      case kNeverMatchedChar:
        DCHECK_EQ(pos_, end_);
        return Fail(HtmlFastPathResult::kFailedParsingQuotedAttributeValue,
                    std::pair{Span{}, USpan{}});
      case '\0':
        DCHECK_EQ(*pos_, '\0');
        // \0 is generally mapped to \uFFFD (but there are exceptions).
        // Fallback to normal path as this generally does not happen often.
        return Fail(HtmlFastPathResult::kFailedParsingQuotedAttributeValue,
                    std::pair{Span{}, USpan{}});
      case '\'':
      case '"': {
        DCHECK(*pos_ == '\'' || *pos_ == '\"');
        Span result =
            Span{initial_start, static_cast<size_t>(pos_ - initial_start)};
        // Consume quote.
        ConsumeNext();
        return {result, USpan{}};
      }
      case '&':
      case '\r':
        DCHECK(*pos_ == '&' || *pos_ == '\r');
        pos_ = initial_start - 1;
        return {Span{}, ScanEscapedAttrValue()};
    };

    NOTREACHED_IN_MIGRATION();
    return {};
  }
#endif  // VECTORIZE_SCANNING

  std::pair<Span, USpan> ScanAttrValue() {
    Span result;
    SkipWhitespace();
    const Char* start = pos_;
    // clang-format off
    if (Char quote_char = GetNext();
        quote_char == '"' || quote_char == '\'') {
      // clang-format on
      start = ++pos_;
#if VECTORIZE_SCANNING
      if (static_cast<size_t>(end_ - pos_) >= kVectorizationThreshold) {
        return ScanAttrValueVectorized(quote_char, start);
      }
#endif  // VECTORIZE_SCANNING
      while (pos_ != end_) {
        uint16_t c = GetNext();
        static_assert('\'' > '\"');
        // The c is mostly like to be a~z or A~Z, the ASCII code value of a~z
        // and A~Z is greater than kSingleQuote, so we just need to compare
        // kSingleQuote here.
        if (c > '\'') [[likely]] {
          ++pos_;
        } else if (c == '&' || c == '\r') {
          pos_ = start - 1;
          return {Span{}, ScanEscapedAttrValue()};
        } else if (c == '\'' || c == '\"') {
          break;
        } else if (c == '\0') [[unlikely]] {
          // \0 is generally mapped to \uFFFD (but there are exceptions).
          // Fallback to normal path as this generally does not happen often.
          return Fail(HtmlFastPathResult::kFailedParsingQuotedAttributeValue,
                      std::pair{Span{}, USpan{}});
        } else {
          ++pos_;
        }
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
    uchar_buffer_.clear();
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
          uchar_buffer_.AddChar('\n');
          ++pos_;
        } else {
          uchar_buffer_.AddChar(*pos_);
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

  void ScanHTMLCharacterReference(UCharLiteralBufferType* out) {
    DCHECK_EQ(*pos_, '&');
    ++pos_;
    const Char* start = pos_;
    while (true) {
      // A rather arbitrary constant to prevent unbounded lookahead in the case
      // of ill-formed input.
      constexpr int kMaxLength = 20;
      if (pos_ == end_ || pos_ - start > kMaxLength) {
        return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
      }
      if (*pos_ == '\0') [[unlikely]] {
        return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
      }
      // Note: the fast path will only parse `;`-terminated character
      // references, and will fail (above) on others, e.g. `A&ampB`.
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
        out->AddChar(entity.data[i]);
      }
      // Handle the most common named references.
    } else if (reference == "amp") {
      out->AddChar('&');
    } else if (reference == "lt") {
      out->AddChar('<');
    } else if (reference == "gt") {
      out->AddChar('>');
    } else if (reference == "nbsp") {
      out->AddChar(0xa0);
    } else {
      // This handles uncommon named references.
      // This does not use `reference` as `reference` does not contain the `;`,
      // which impacts behavior of ConsumeHTMLEntity().
      String input_string{start, static_cast<unsigned>(pos_ - start)};
      SegmentedString input_segmented{input_string};
      DecodedHTMLEntity entity;
      bool not_enough_characters = false;
      if (!ConsumeHTMLEntity(input_segmented, entity, not_enough_characters) ||
          not_enough_characters) {
        return Fail(HtmlFastPathResult::kFailedParsingCharacterReference);
      }
      for (size_t i = 0; i < entity.length; ++i) {
        out->AddChar(entity.data[i]);
      }
      // ConsumeHTMLEntity() may not have consumed all the input.
      const unsigned remaining_length = input_segmented.length();
      if (remaining_length) {
        pos_ -= remaining_length;
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
      ScanTextResult<Char> scanned_text = ScanText();
      if (failed_) {
        return;
      }
      DCHECK(scanned_text.text.empty() || !scanned_text.escaped_text);
      if (!scanned_text.text.empty()) {
        const auto text = scanned_text.text;
        if (text.size() >= Text::kDefaultLengthLimit) {
          return Fail(HtmlFastPathResult::kFailedBigText);
        }
        parent->ParserAppendChildInDocumentFragment(
            Text::Create(document_, scanned_text.TryCanonicalizeString()));
      } else if (scanned_text.escaped_text) {
        if (scanned_text.escaped_text->size() >= Text::kDefaultLengthLimit) {
          return Fail(HtmlFastPathResult::kFailedBigText);
        }
        parent->ParserAppendChildInDocumentFragment(
            Text::Create(document_, scanned_text.escaped_text->AsString()));
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
        parent->ParserAppendChildInDocumentFragment(child);
      }
    }
  }

  Attribute ProcessAttribute(Span name_span,
                             std::pair<Span, USpan> value_span) {
    QualifiedName name = LookupHTMLAttributeName(
        name_span.data(), static_cast<unsigned>(name_span.size()));
    if (name == g_null_name) {
      name = QualifiedName(AtomicString(name_span));
    }

    // The string pointer in |value| is null for attributes with no values, but
    // the null atom is used to represent absence of attributes; attributes with
    // no values have the value set to an empty atom instead.
    AtomicString value;
    if (value_span.second.empty()) {
      value = AtomicString(value_span.first);
    } else {
      value = AtomicString(value_span.second);
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
      if (attr_name.size() > 2 && attr_name[0] == 'o' && attr_name[1] == 'n') {
        // These attributes likely contain script that may be executed at random
        // points, which could cause problems if parsing via the fast path
        // fails. For example, an image's onload event.
        return Fail(HtmlFastPathResult::kFailedOnAttribute);
      }
      if (attr_name.size() == 2 && attr_name[0] == 'i' && attr_name[1] == 's') {
        // This is for the "is" attribute case.
        return Fail(HtmlFastPathResult::kFailedParsingAttributes);
      }
      if (GetNext() != '=') {
        SkipWhitespace();
      }
      std::pair<Span, USpan> attr_value = {};
      if (GetNext() == '=') {
        ++pos_;
        attr_value = ScanAttrValue();
        SkipWhitespace();
      }
      Attribute attribute = ProcessAttribute(attr_name, attr_value);
      attribute_names_.push_back(attribute.LocalName().Impl());
      attribute_buffer_.push_back(std::move(attribute));
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
    // Clang has a hard time formatting this, disable clang format.
    // clang-format off
#define TAG_CASE(Tagname)                                                      \
    case TagnameHash(TagInfo::Tagname::tagname):                               \
      if constexpr (non_phrasing_content                                       \
                      ? TagInfo::Tagname::AllowedInFlowContent()               \
                      : TagInfo::Tagname::AllowedInPhrasingOrFlowContent()) {  \
        /* See comment in Run() for details on why equality is checked */      \
        /* here. */                                                            \
        if (tagname == TagInfo::Tagname::tagname) {                            \
          return ParseElementAfterTagname<typename TagInfo::Tagname>();        \
        }                                                                      \
      }                                                                        \
      break;

    switch (TagnameHash(tagname)) {
      case TagnameHash(TagInfo::A::tagname):
        // <a> tags must not be nested, because HTML parsing would auto-close
        // the outer one when encountering a nested one.
        if (tagname == TagInfo::A::tagname && !inside_of_tag_a_) {
          return non_phrasing_content
                     ? ParseElementAfterTagname<typename TagInfo::A>()
                     : ParseElementAfterTagname<
                           typename TagInfo::AWithPhrasingContent>();
        }
        break;
      TAG_CASE(B)
      TAG_CASE(Br)
      TAG_CASE(Button)
      TAG_CASE(Div)
      TAG_CASE(Footer)
      TAG_CASE(I)
      TAG_CASE(Input)
      case TagnameHash(TagInfo::Li::tagname):
        if constexpr (non_phrasing_content
                          ? TagInfo::Li::AllowedInFlowContent()
                          : TagInfo::Li::AllowedInPhrasingOrFlowContent()) {
          // See comment in Run() for details on why equality is checked here.
          // <li>s autoclose when multiple are encountered. For example,
          // <li><li></li></li> results in sibling <li>s, not nested <li>s. Fail
          // in such a case.
          if (tagname == TagInfo::Li::tagname && !inside_of_tag_li_) {
            inside_of_tag_li_ = true;
            Element* result = ParseElementAfterTagname<typename TagInfo::Li>();
            inside_of_tag_li_ = false;
            return result;
          }
        }
        break;
      TAG_CASE(Label)
      TAG_CASE(Option)
      TAG_CASE(Ol)
      TAG_CASE(P)
      TAG_CASE(Select)
      TAG_CASE(Span)
      TAG_CASE(Strong)
      TAG_CASE(Ul)
#undef TAG_CASE
      default:
        break;
    }
    // clang-format on
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
    element->BeginParsingChildren();
    ParseChildren<Tag>(element);
    if (failed_ || pos_ == end_) {
      return Fail(HtmlFastPathResult::kFailedEndOfInputReachedForContainer,
                  element);
    }
    // ParseChildren<Tag>(element) stops after the (hopefully) closing tag's `<`
    // and fails if the the current char is not '/'.
    DCHECK_EQ(*pos_, '/');
    ++pos_;
    // -1 as the name includes \0.
    const size_t tag_length = std::size(Tag::tagname) - 1;
    DCHECK_LE(pos_, end_);
    // <= as there needs to be a '>'.
    if (static_cast<size_t>(end_ - pos_) <= tag_length) {
      return Fail(HtmlFastPathResult::kFailedUnexpectedTagNameCloseState,
                  element);
    }
    Span tag_name_span(pos_, tag_length);
    pos_ += tag_length;
    if (tag_name_span == Tag::tagname ||
        SpanMatchesLowercase(tag_name_span, Tag::tagname)) {
      SkipWhitespace();
      if (ConsumeNext() != '>') {
        return Fail(HtmlFastPathResult::kFailedUnexpectedTagNameCloseState,
                    element);
      }
    } else {
      return Fail(HtmlFastPathResult::kFailedEndTagNameMismatch, element);
    }
    element->FinishParsingChildren();
    return element;
  }

  Element* ParseVoidElement(Element* element) {
    ParseAttributes(element);
    if (failed_) {
      return element;
    }
    element->BeginParsingChildren();
    element->FinishParsingChildren();
    return element;
  }
};

void LogFastPathResult(HtmlFastPathResult result) {
  UMA_HISTOGRAM_ENUMERATION("Blink.HTMLFastPathParser.ParseResult", result);
  if (result != HtmlFastPathResult::kSucceeded) {
    VLOG(2) << "innerHTML fast-path parser failed, "
            << static_cast<int>(result);
  }
}

bool CanUseFastPath(Document& document,
                    Element& context_element,
                    ParserContentPolicy policy,
                    HTMLFragmentParsingBehaviorSet behavior) {
  if (behavior.Has(HTMLFragmentParsingBehavior::kIncludeShadowRoots)) {
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
  auto* template_element = DynamicTo<HTMLTemplateElement>(context_element);
  if (!template_element && Traversal<HTMLFormElement>::FirstAncestorOrSelf(
                               context_element) != nullptr) {
    LogFastPathResult(HtmlFastPathResult::kFailedInForm);
    return false;
  }

  // TODO(crbug.com/1453291) For now, declarative DOM Parts are not supported by
  // the fast path parser.
  if (RuntimeEnabledFeatures::DOMPartsAPIEnabled() && template_element &&
      template_element->hasAttribute(html_names::kParsepartsAttr)) {
    LogFastPathResult(HtmlFastPathResult::kFailedUnsupportedContextTag);
    return false;
  }
  return true;
}

// A hand picked enumeration of the most frequently used tags on web pages with
// some amount of grouping. Ranking comes from
// (https://discuss.httparchive.org/t/use-of-html-elements/1438).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused (unless the histogram name is
// updated).
enum class UnsupportedTagType : uint32_t {
  // The tag is supported.
  kSupported = 0,
  kImg = 1 << 0,
  kAside = 1 << 1,
  kU = 1 << 2,
  kHr = 1 << 3,
  // This is h1-h6.
  kH = 1 << 4,
  kEm = 1 << 5,
  // The tag is not html.
  kNotHtml = 1 << 6,
  // The tag is a known html tag, but not one covered by this enum.
  kOtherHtml = 1 << 7,
  kForm = 1 << 8,
  // This includes header, footer, and section.
  kArticleLike = 1 << 9,
  kNav = 1 << 10,
  kIFrame = 1 << 11,
  // This includes tr, td, tbody, th.
  kTableLike = 1 << 12,
  // This includes dl, dt, dd.
  kDescriptionList = 1 << 13,
  kIns = 1 << 14,
  kBlockquote = 1 << 15,
  kCenter = 1 << 16,
  kSmall = 1 << 17,
  kFont = 1 << 18,
  kFieldset = 1 << 19,
  kTextarea = 1 << 20,
  kTime = 1 << 21,
  kSvg = 1 << 22,
  kBody = 1 << 23,
  kMaxValue = kBody,
};

constexpr uint32_t kAllUnsupportedTags =
    (static_cast<uint32_t>(UnsupportedTagType::kMaxValue) << 1) - 1;
// If UnsupportedTagType is > 24, then need to add a fourth chunk to the
// overall histogram.
static_assert(kAllUnsupportedTags < (1 << 24));

#define CHECK_TAG_TYPE(t)                       \
  if (node.HasTagName(html_names::k##t##Tag)) { \
    return UnsupportedTagType::k##t;            \
  }

#define NODE_HAS_TAG_NAME(t) node.HasTagName(html_names::k##t##Tag) ||

// Returns the UnsupportedTagType for node. Returns 0 if `node` is one of the
// supported tags.
UnsupportedTagType UnsupportedTagTypeValueForNode(const Node& node) {
  // "false" is needed as NODE_HAS_TAG_NAME has a trailing '||'. Without it,
  // would get compile errors.
  const bool hack_for_macro_to_work_in_conditional = false;
  if (SUPPORTED_TAGS(NODE_HAS_TAG_NAME) hack_for_macro_to_work_in_conditional) {
    // Known tag.
    return UnsupportedTagType::kSupported;
  }
  if (node.HasTagName(html_names::kH1Tag) ||
      node.HasTagName(html_names::kH2Tag) ||
      node.HasTagName(html_names::kH3Tag) ||
      node.HasTagName(html_names::kH4Tag) ||
      node.HasTagName(html_names::kH5Tag) ||
      node.HasTagName(html_names::kH6Tag)) {
    return UnsupportedTagType::kH;
  }
  if (node.HasTagName(html_names::kArticleTag) ||
      node.HasTagName(html_names::kHeaderTag) ||
      node.HasTagName(html_names::kFooterTag) ||
      node.HasTagName(html_names::kSectionTag)) {
    return UnsupportedTagType::kArticleLike;
  }
  if (node.HasTagName(html_names::kTableTag) ||
      node.HasTagName(html_names::kTrTag) ||
      node.HasTagName(html_names::kTdTag) ||
      node.HasTagName(html_names::kTbodyTag) ||
      node.HasTagName(html_names::kThTag)) {
    return UnsupportedTagType::kTableLike;
  }
  if (node.HasTagName(html_names::kDlTag) ||
      node.HasTagName(html_names::kDtTag) ||
      node.HasTagName(html_names::kDdTag)) {
    return UnsupportedTagType::kDescriptionList;
  }
  if (node.HasTagName(svg_names::kSVGTag)) {
    return UnsupportedTagType::kSvg;
  }
  CHECK_TAG_TYPE(Aside)
  CHECK_TAG_TYPE(U)
  CHECK_TAG_TYPE(Hr)
  CHECK_TAG_TYPE(Em)
  CHECK_TAG_TYPE(Form)
  CHECK_TAG_TYPE(Nav)
  CHECK_TAG_TYPE(IFrame)
  CHECK_TAG_TYPE(Ins)
  CHECK_TAG_TYPE(Blockquote)
  CHECK_TAG_TYPE(Center)
  CHECK_TAG_TYPE(Small)
  CHECK_TAG_TYPE(Font)
  CHECK_TAG_TYPE(Fieldset)
  CHECK_TAG_TYPE(Textarea)
  CHECK_TAG_TYPE(Time)
  CHECK_TAG_TYPE(Body)
  if (node.IsHTMLElement() && To<Element>(node).TagQName().IsDefinedName()) {
    return UnsupportedTagType::kOtherHtml;
  }
  return UnsupportedTagType::kNotHtml;
}

// Histogram names used when logging unsupported tag type.
const char* kUnsupportedTagTypeCompositeName =
    "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2";
const char* kUnsupportedTagTypeMaskNames[] = {
    "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2",
    "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2",
    "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2",
};

// Histogram names used when logging unsupported context tag type.
const char* kUnsupportedContextTagTypeCompositeName =
    "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2";
const char* kUnsupportedContextTagTypeMaskNames[] = {
    "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask0V2",
    "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask1V2",
    "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask2V2",
};

// Logs histograms for either an unsupported tag or unsupported context tag.
// `type_mask` is a bitmask of the unsupported tags that were encountered. As
// the uma frontend doesn't handle large bitmasks well, there are 4 separate
// histograms logged:
// . histogram for bits 1-8, 9-16, 17-24. The names used for these histograms
//   is specified in `mask_histogram_names`.
// . A histogram identifying which bit ranges of `type_mask` have at least one
//   bit set. More specifically:
//   . bit 1 set if `type_mask` has at least one bit set in bits 1-8.
//   . bit 2 set if `type_mask` has at least one bit set in bits 9-16.
//   . bit 3 set if `type_mask` has at least one bit set in bits 17-24.
void LogFastPathUnsupportedTagTypeDetails(uint32_t type_mask,
                                          const char* composite_histogram_name,
                                          const char* mask_histogram_names[]) {
  // This should only be called once an unsupported tag is encountered.
  DCHECK_NE(static_cast<uint32_t>(0), type_mask);
  uint32_t chunk_mask = 0;
  if ((type_mask & 0xFF) != 0) {
    chunk_mask |= 1;
    base::UmaHistogramExactLinear(mask_histogram_names[0], type_mask & 0xFF,
                                  256);
  }
  if (((type_mask >> 8) & 0xFF) != 0) {
    chunk_mask |= 2;
    base::UmaHistogramExactLinear(mask_histogram_names[1],
                                  (type_mask >> 8) & 0xFF, 256);
  }
  if (((type_mask >> 16) & 0xFF) != 0) {
    chunk_mask |= 4;
    base::UmaHistogramExactLinear(mask_histogram_names[2],
                                  (type_mask >> 16) & 0xFF, 256);
  }
  base::UmaHistogramExactLinear(composite_histogram_name, chunk_mask, 8);
}

template <class Char>
bool TryParsingHTMLFragmentImpl(const base::span<const Char>& source,
                                Document& document,
                                ContainerNode& root_node,
                                Element& context_element,
                                HTMLFragmentParsingBehaviorSet behavior,
                                bool* failed_because_unsupported_tag) {
  base::ElapsedTimer parse_timer;
  int number_of_bytes_parsed;
  HTMLFastPathParser<Char> parser{source, document, root_node};
  const bool success = parser.Run(context_element, behavior);
  LogFastPathResult(parser.parse_result());
  number_of_bytes_parsed = parser.NumberOfBytesParsed();
  // The time needed to parse is typically < 1ms (even at the 99%).
  if (success) {
    root_node.ParserFinishedBuildingDocumentFragment();
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLFastPathParser.SuccessfulParseTime2", parse_timer.Elapsed(),
        base::Microseconds(1), base::Milliseconds(10), 100);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLFastPathParser.AbortedParseTime2", parse_timer.Elapsed(),
        base::Microseconds(1), base::Milliseconds(10), 100);
  }
  if (failed_because_unsupported_tag) {
    *failed_because_unsupported_tag =
        parser.parse_result() == HtmlFastPathResult::kFailedUnsupportedTag;
  }
  if (parser.parse_result() ==
          HtmlFastPathResult::kFailedUnsupportedContextTag &&
      RuntimeEnabledFeatures::InnerHTMLParserFastpathLogFailureEnabled()) {
    const UnsupportedTagType context_tag_type =
        UnsupportedTagTypeValueForNode(context_element);
    // If the context element isn't a valid container but is supported
    // UnsupportedTagTypeValueForNode() will return kSupported. For now this is
    // really only <br>. I suspect this is extremely rare, so don't log for now.
    if (context_tag_type != UnsupportedTagType::kSupported) {
      LogFastPathUnsupportedTagTypeDetails(
          static_cast<uint32_t>(context_tag_type),
          kUnsupportedContextTagTypeCompositeName,
          kUnsupportedContextTagTypeMaskNames);
    }
  }
  if (success) {
    UMA_HISTOGRAM_COUNTS_10M("Blink.HTMLFastPathParser.SuccessfulParseSize",
                             number_of_bytes_parsed);
  } else {
    UMA_HISTOGRAM_COUNTS_10M("Blink.HTMLFastPathParser.AbortedParseSize",
                             number_of_bytes_parsed);
  }
  return success;
}

}  // namespace

bool TryParsingHTMLFragment(const String& source,
                            Document& document,
                            ContainerNode& parent,
                            Element& context_element,
                            ParserContentPolicy policy,
                            HTMLFragmentParsingBehaviorSet behavior,
                            bool* failed_because_unsupported_tag) {
  if (!CanUseFastPath(document, context_element, policy, behavior)) {
    return false;
  }
  return source.Is8Bit()
             ? TryParsingHTMLFragmentImpl<LChar>(
                   source.Span8(), document, parent, context_element, behavior,
                   failed_because_unsupported_tag)
             : TryParsingHTMLFragmentImpl<UChar>(
                   source.Span16(), document, parent, context_element, behavior,
                   failed_because_unsupported_tag);
}

void LogTagsForUnsupportedTagTypeFailure(DocumentFragment& fragment) {
  uint32_t type_mask = 0u;
  Node* node = NodeTraversal::Next(fragment);
  while (node && type_mask != kAllUnsupportedTags) {
    type_mask |= static_cast<uint32_t>(UnsupportedTagTypeValueForNode(*node));
    node = NodeTraversal::Next(*node);
  }
  // The mask may still be 0 in some cases, such as empty text, or tags that
  // don't create nodes (frameset).
  if (type_mask != 0) {
    LogFastPathUnsupportedTagTypeDetails(type_mask,
                                         kUnsupportedTagTypeCompositeName,
                                         kUnsupportedTagTypeMaskNames);
  }
}

#undef SUPPORTED_TAGS

}  // namespace blink
