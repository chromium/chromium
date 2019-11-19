/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_tree_builder_simulator.h"

#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

static bool TokenExitsForeignContent(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLTreeBuilder::processTokenInForeignContent
  // and changed to use threadSafeHTMLNamesMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, html_names::kBTag) ||
         ThreadSafeMatch(tag_name, html_names::kBigTag) ||
         ThreadSafeMatch(tag_name, html_names::kBlockquoteTag) ||
         ThreadSafeMatch(tag_name, html_names::kBodyTag) ||
         ThreadSafeMatch(tag_name, html_names::kBrTag) ||
         ThreadSafeMatch(tag_name, html_names::kCenterTag) ||
         ThreadSafeMatch(tag_name, html_names::kCodeTag) ||
         ThreadSafeMatch(tag_name, html_names::kDdTag) ||
         ThreadSafeMatch(tag_name, html_names::kDivTag) ||
         ThreadSafeMatch(tag_name, html_names::kDlTag) ||
         ThreadSafeMatch(tag_name, html_names::kDtTag) ||
         ThreadSafeMatch(tag_name, html_names::kEmTag) ||
         ThreadSafeMatch(tag_name, html_names::kEmbedTag) ||
         ThreadSafeMatch(tag_name, html_names::kH1Tag) ||
         ThreadSafeMatch(tag_name, html_names::kH2Tag) ||
         ThreadSafeMatch(tag_name, html_names::kH3Tag) ||
         ThreadSafeMatch(tag_name, html_names::kH4Tag) ||
         ThreadSafeMatch(tag_name, html_names::kH5Tag) ||
         ThreadSafeMatch(tag_name, html_names::kH6Tag) ||
         ThreadSafeMatch(tag_name, html_names::kHeadTag) ||
         ThreadSafeMatch(tag_name, html_names::kHrTag) ||
         ThreadSafeMatch(tag_name, html_names::kITag) ||
         ThreadSafeMatch(tag_name, html_names::kImgTag) ||
         ThreadSafeMatch(tag_name, html_names::kLiTag) ||
         ThreadSafeMatch(tag_name, html_names::kListingTag) ||
         ThreadSafeMatch(tag_name, html_names::kMenuTag) ||
         ThreadSafeMatch(tag_name, html_names::kMetaTag) ||
         ThreadSafeMatch(tag_name, html_names::kNobrTag) ||
         ThreadSafeMatch(tag_name, html_names::kOlTag) ||
         ThreadSafeMatch(tag_name, html_names::kPTag) ||
         ThreadSafeMatch(tag_name, html_names::kPreTag) ||
         ThreadSafeMatch(tag_name, html_names::kRubyTag) ||
         ThreadSafeMatch(tag_name, html_names::kSTag) ||
         ThreadSafeMatch(tag_name, html_names::kSmallTag) ||
         ThreadSafeMatch(tag_name, html_names::kSpanTag) ||
         ThreadSafeMatch(tag_name, html_names::kStrongTag) ||
         ThreadSafeMatch(tag_name, html_names::kStrikeTag) ||
         ThreadSafeMatch(tag_name, html_names::kSubTag) ||
         ThreadSafeMatch(tag_name, html_names::kSupTag) ||
         ThreadSafeMatch(tag_name, html_names::kTableTag) ||
         ThreadSafeMatch(tag_name, html_names::kTtTag) ||
         ThreadSafeMatch(tag_name, html_names::kUTag) ||
         ThreadSafeMatch(tag_name, html_names::kUlTag) ||
         ThreadSafeMatch(tag_name, html_names::kVarTag) ||
         (ThreadSafeMatch(tag_name, html_names::kFontTag) &&
          (token.GetAttributeItem(html_names::kColorAttr) ||
           token.GetAttributeItem(html_names::kFaceAttr) ||
           token.GetAttributeItem(html_names::kSizeAttr)));
}

static bool TokenExitsMath(const CompactHTMLToken& token) {
  // FIXME: This is copied from HTMLElementStack::isMathMLTextIntegrationPoint
  // and changed to use threadSafeMatch.
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, mathml_names::kMiTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMoTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMnTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMsTag) ||
         ThreadSafeMatch(tag_name, mathml_names::kMtextTag);
}

static bool TokenExitsInSelect(const CompactHTMLToken& token) {
  // https://html.spec.whatwg.org/C/#parsing-main-inselect
  const String& tag_name = token.Data();
  return ThreadSafeMatch(tag_name, html_names::kInputTag) ||
         ThreadSafeMatch(tag_name, html_names::kKeygenTag) ||
         ThreadSafeMatch(tag_name, html_names::kTextareaTag);
}

HTMLTreeBuilderSimulator::HTMLTreeBuilderSimulator(
    const HTMLParserOptions& options)
    : options_(options), in_select_insertion_mode_(false) {
  namespace_stack_.push_back(HTML);
}

HTMLTreeBuilderSimulator::State HTMLTreeBuilderSimulator::StateFor(
    HTMLTreeBuilder* tree_builder) {
  DCHECK(IsMainThread());
  State namespace_stack;
  for (HTMLElementStack::ElementRecord* record =
           tree_builder->OpenElements()->TopRecord();
       record; record = record->Next()) {
    Namespace current_namespace = HTML;
    if (record->NamespaceURI() == svg_names::kNamespaceURI)
      current_namespace = SVG;
    else if (record->NamespaceURI() == mathml_names::kNamespaceURI)
      current_namespace = kMathML;

    if (namespace_stack.IsEmpty() ||
        namespace_stack.back() != current_namespace)
      namespace_stack.push_back(current_namespace);
  }
  namespace_stack.Reverse();
  return namespace_stack;
}

HTMLTreeBuilderSimulator::SimulatedToken HTMLTreeBuilderSimulator::Simulate(
    const CompactHTMLToken& token,
    HTMLTokenizer* tokenizer) {
  SimulatedToken simulated_token = kOtherToken;

  if (token.GetType() == HTMLToken::kStartTag) {
    const String& tag_name = token.Data();
    if (ThreadSafeMatch(tag_name, svg_names::kSVGTag))
      namespace_stack_.push_back(SVG);
    if (ThreadSafeMatch(tag_name, mathml_names::kMathTag))
      namespace_stack_.push_back(kMathML);
    if (InForeignContent() && TokenExitsForeignContent(token))
      namespace_stack_.pop_back();
    if (IsHTMLIntegrationPointForStartTag(token) ||
        (namespace_stack_.back() == kMathML && TokenExitsMath(token))) {
      namespace_stack_.push_back(HTML);
    } else if (!InForeignContent()) {
      // FIXME: This is just a copy of Tokenizer::updateStateFor which uses
      // threadSafeMatches.
      if (ThreadSafeMatch(tag_name, html_names::kTextareaTag) ||
          ThreadSafeMatch(tag_name, html_names::kTitleTag)) {
        tokenizer->SetState(HTMLTokenizer::kRCDATAState);
      } else if (ThreadSafeMatch(tag_name, html_names::kScriptTag)) {
        tokenizer->SetState(HTMLTokenizer::kScriptDataState);

        String type_attribute_value;
        if (auto* item = token.GetAttributeItem(html_names::kTypeAttr)) {
          type_attribute_value = item->Value();
        }

        String language_attribute_value;
        if (auto* item = token.GetAttributeItem(html_names::kLanguageAttr)) {
          language_attribute_value = item->Value();
        }

        if (ScriptLoader::IsValidScriptTypeAndLanguage(
                type_attribute_value, language_attribute_value,
                ScriptLoader::kAllowLegacyTypeInTypeAttribute)) {
          simulated_token = kValidScriptStart;
        }
      } else if (ThreadSafeMatch(tag_name, html_names::kLinkTag)) {
        simulated_token = kLink;
      } else if (!in_select_insertion_mode_) {
        // If we're in the "in select" insertion mode, all of these tags are
        // ignored, so we shouldn't change the tokenizer state:
        // https://html.spec.whatwg.org/C/#parsing-main-inselect
        if (ThreadSafeMatch(tag_name, html_names::kPlaintextTag) &&
            !in_select_insertion_mode_) {
          tokenizer->SetState(HTMLTokenizer::kPLAINTEXTState);
        } else if (ThreadSafeMatch(tag_name, html_names::kStyleTag) ||
                   ThreadSafeMatch(tag_name, html_names::kIFrameTag) ||
                   ThreadSafeMatch(tag_name, html_names::kXmpTag) ||
                   ThreadSafeMatch(tag_name, html_names::kNoembedTag) ||
                   ThreadSafeMatch(tag_name, html_names::kNoframesTag) ||
                   (ThreadSafeMatch(tag_name, html_names::kNoscriptTag) &&
                    options_.script_enabled)) {
          tokenizer->SetState(HTMLTokenizer::kRAWTEXTState);
        }
      }

      // We need to track whether we're in the "in select" insertion mode
      // in order to determine whether '<plaintext>' will put the tokenizer
      // into PLAINTEXTState, and whether '<xmp>' and others will consume
      // textual content.
      //
      // https://html.spec.whatwg.org/C/#parsing-main-inselect
      if (ThreadSafeMatch(tag_name, html_names::kSelectTag)) {
        in_select_insertion_mode_ = true;
      } else if (in_select_insertion_mode_ && TokenExitsInSelect(token)) {
        in_select_insertion_mode_ = false;
      }
    }
  }

  if (token.GetType() == HTMLToken::kEndTag ||
      (token.GetType() == HTMLToken::kStartTag && token.SelfClosing() &&
       InForeignContent())) {
    const String& tag_name = token.Data();
    if ((namespace_stack_.back() == SVG &&
         ThreadSafeMatch(tag_name, svg_names::kSVGTag)) ||
        (namespace_stack_.back() == kMathML &&
         ThreadSafeMatch(tag_name, mathml_names::kMathTag)) ||
        IsHTMLIntegrationPointForEndTag(token) ||
        (namespace_stack_.Contains(kMathML) &&
         namespace_stack_.back() == HTML && TokenExitsMath(token))) {
      namespace_stack_.pop_back();
    }
    if (ThreadSafeMatch(tag_name, html_names::kScriptTag)) {
      if (!InForeignContent())
        tokenizer->SetState(HTMLTokenizer::kDataState);
      return kScriptEnd;
    }
    if (ThreadSafeMatch(tag_name, html_names::kSelectTag))
      in_select_insertion_mode_ = false;
    if (ThreadSafeMatch(tag_name, html_names::kStyleTag))
      simulated_token = kStyleEnd;
  }
  if (token.GetType() == HTMLToken::kStartTag &&
      simulated_token == kOtherToken) {
    const String& tag_name = token.Data();
    // Use the presence of a dash in the tag name as a proxy for
    // "is a custom element".
    if (tag_name.find('-') != kNotFound)
      simulated_token = kCustomElementBegin;
  }

  // FIXME: Also setForceNullCharacterReplacement when in text mode.
  tokenizer->SetForceNullCharacterReplacement(InForeignContent());
  tokenizer->SetShouldAllowCDATA(InForeignContent());
  return simulated_token;
}

// https://html.spec.whatwg.org/C/#html-integration-point
bool HTMLTreeBuilderSimulator::IsHTMLIntegrationPointForStartTag(
    const CompactHTMLToken& token) const {
  DCHECK(token.GetType() == HTMLToken::kStartTag) << token.GetType();

  Namespace tokens_ns = namespace_stack_.back();
  const String& tag_name = token.Data();
  if (tokens_ns == kMathML) {
    if (!ThreadSafeMatch(tag_name, mathml_names::kAnnotationXmlTag))
      return false;
    if (auto* encoding = token.GetAttributeItem(mathml_names::kEncodingAttr)) {
      return EqualIgnoringASCIICase(encoding->Value(), "text/html") ||
             EqualIgnoringASCIICase(encoding->Value(), "application/xhtml+xml");
    }
  } else if (tokens_ns == SVG) {
    // FIXME: It's very fragile that we special case foreignObject here to be
    // case-insensitive.
    if (DeprecatedEqualIgnoringCase(tag_name,
                                    svg_names::kForeignObjectTag.LocalName()))
      return true;
    return ThreadSafeMatch(tag_name, svg_names::kDescTag) ||
           ThreadSafeMatch(tag_name, svg_names::kTitleTag);
  }
  return false;
}

// https://html.spec.whatwg.org/C/#html-integration-point
bool HTMLTreeBuilderSimulator::IsHTMLIntegrationPointForEndTag(
    const CompactHTMLToken& token) const {
  if (token.GetType() != HTMLToken::kEndTag)
    return false;

  // If it's inside an HTML integration point, the top namespace is
  // HTML, and its next namespace is not HTML.
  if (namespace_stack_.back() != HTML)
    return false;
  if (namespace_stack_.size() < 2)
    return false;
  Namespace tokens_ns = namespace_stack_[namespace_stack_.size() - 2];

  const String& tag_name = token.Data();
  if (tokens_ns == kMathML)
    return ThreadSafeMatch(tag_name, mathml_names::kAnnotationXmlTag);
  if (tokens_ns == SVG) {
    // FIXME: It's very fragile that we special case foreignObject here to be
    // case-insensitive.
    if (DeprecatedEqualIgnoringCase(tag_name,
                                    svg_names::kForeignObjectTag.LocalName()))
      return true;
    return ThreadSafeMatch(tag_name, svg_names::kDescTag) ||
           ThreadSafeMatch(tag_name, svg_names::kTitleTag);
  }
  return false;
}

}  // namespace blink
