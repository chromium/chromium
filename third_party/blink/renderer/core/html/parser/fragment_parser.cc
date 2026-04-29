// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/fragment_parser.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sethtmlunsafeoptions_trustedparseroptions.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

void LogFastPathParserTotalTime(base::TimeDelta parse_time) {
  // The time needed to parse is typically < 1ms (even at the 99%).
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLFastPathParser.TotalParseTime2", parse_time,
      base::Microseconds(1), base::Milliseconds(10), 100);
}

// ForceInertTemplate specifies whether the HTML parser should parse into an
// inert (non-active) template document.
enum class ForceInertTemplate { kDontForce, kForce };

DocumentFragment* ParseHTMLFragmentInternal(
    const String& markup,
    Element* context_element,
    ParserContentPolicy parser_content_policy,
    FragmentParserConfig::ParseDeclarativeShadowRoots parse_declarative_shadows,
    FragmentParserConfig::ForceHtml force_html,
    ForceInertTemplate force_inert,
    CustomElementRegistry* registry,
    ExceptionState& exception_state,
    StreamingSanitizer* sanitizer) {
  DCHECK(context_element);
  const HTMLTemplateElement* template_element =
      DynamicTo<HTMLTemplateElement>(*context_element);
  if (template_element && !template_element->GetExecutionContext()) {
    return nullptr;
  }

  // If an inert document is requested, we shouldn't run custom element
  // callbacks. Those will be run whenever the result template will be inserted
  // into the final document.
  if (force_inert == ForceInertTemplate::kForce) {
    registry = nullptr;
  }

  Document& document =
      (IsA<HTMLTemplateElement>(*context_element) ||
       force_inert == ForceInertTemplate::kForce)
          ? context_element->GetDocument().EnsureTemplateDocument()
          : context_element->GetDocument();
  DocumentFragment* fragment = DocumentFragment::Create(document);
  document.setAllowDeclarativeShadowRoots(
      parse_declarative_shadows ==
      FragmentParserConfig::ParseDeclarativeShadowRoots::kParse);

  if (IsA<HTMLDocument>(document) ||
      force_html == FragmentParserConfig::ForceHtml::kForce) {
    bool log_tag_stats = false;
    base::ElapsedTimer parse_timer;
    HTMLFragmentParsingBehaviorSet parser_behavior;
    if (parse_declarative_shadows ==
        FragmentParserConfig::ParseDeclarativeShadowRoots::kParse) {
      parser_behavior.Put(HTMLFragmentParsingBehavior::kIncludeShadowRoots);
    }
    const bool parsed_fast_path =
        !sanitizer &&
        TryParsingHTMLFragment(markup, document, *fragment, *context_element,
                               parser_content_policy, parser_behavior,
                               &log_tag_stats);
    if (parsed_fast_path) {
      fragment->SetHoldsUnnotifiedChildren(true);
      fragment->ParserFinishedBuildingDocumentFragment(
          DocumentFragment::ShouldNotifyInsertedNodes::kSkip);
      // If parsed by fast path, no upgrade will be happening so we can simply
      // set the custom element registry to the new elements to keep track.
      // We attempt to optimize the registry setting by checking if the
      // newly-created elements are using the same registry as the tree scope.
      // If they're the same, we don't need to set registry on the descendants
      // as the descendants can look up the registry from tree scope like usual.
      if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled() &&
          registry != context_element->GetTreeScope().customElementRegistry()) {
        for (Element& element : ElementTraversal::DescendantsOf(*fragment)) {
          element.SetCustomElementRegistry(registry);
        }
      }
      LogFastPathParserTotalTime(parse_timer.Elapsed());
#if DCHECK_IS_ON()
      // As a sanity check for the fast-path, create another fragment using
      // the full parser and compare the results.
      // See https://bugs.chromium.org/p/chromium/issues/detail?id=1407201
      // for details.
      DocumentFragment* fragment2 = DocumentFragment::Create(document);
      fragment2->ParseHTML(markup, context_element, registry,
                           parser_content_policy, sanitizer);
      DCHECK_EQ(CreateMarkup(fragment), CreateMarkup(fragment2))
          << " supplied value " << markup;
      DCHECK(fragment->isEqualNode(fragment2));
#endif
      return fragment;
    }
    fragment = DocumentFragment::Create(document);
    fragment->ParseHTML(markup, context_element, registry,
                        parser_content_policy, sanitizer);
    LogFastPathParserTotalTime(parse_timer.Elapsed());
    if (log_tag_stats &&
        RuntimeEnabledFeatures::InnerHTMLParserFastpathLogFailureEnabled()) {
      LogTagsForUnsupportedTagTypeFailure(*fragment);
    }
    return fragment;
  }

  bool was_valid = fragment->ParseXML(markup, context_element, exception_state,
                                      parser_content_policy);
  if (!was_valid) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The provided markup is invalid XML, and "
        "therefore cannot be inserted into an XML "
        "document.");
    return nullptr;
  }
  return fragment;
}

inline void RemoveElementPreservingChildren(DocumentFragment* fragment,
                                            HTMLElement* element) {
  Node* next_child = nullptr;
  for (Node* child = element->firstChild(); child; child = next_child) {
    next_child = child->nextSibling();
    element->RemoveChild(child);
    fragment->InsertBefore(child, element);
  }
  fragment->RemoveChild(element);
}

}  // namespace

FragmentParserOptions::FragmentParserOptions(TrustedParserOptions* options)
    : trust_mode_(TrustMode::kTrusted),
      run_scripts_((options->runScripts() &&
                    RuntimeEnabledFeatures::SetHTMLCanRunScriptsEnabled())
                       ? RunScripts::kRunScripts
                       : RunScripts::kDontRunScripts),
      sanitizer_init_(options->sanitizer()) {}

FragmentParserOptions::FragmentParserOptions(SetHTMLUnsafeOptions* options)
    : run_scripts_((options->runScripts() &&
                    RuntimeEnabledFeatures::SetHTMLCanRunScriptsEnabled())
                       ? RunScripts::kRunScripts
                       : RunScripts::kDontRunScripts),
      sanitizer_init_(options->sanitizer()) {}

FragmentParserOptions::FragmentParserOptions(SetHTMLOptions* options)
    : sanitizer_init_(options->sanitizer()) {}

// static
FragmentParserOptions FragmentParserOptions::From(
    const V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions* options) {
  switch (options->GetContentType()) {
    case V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions::ContentType::
        kSetHTMLUnsafeOptions:
      return FragmentParserOptions(options->GetAsSetHTMLUnsafeOptions());
    case V8UnionSetHTMLUnsafeOptionsOrTrustedParserOptions::ContentType::
        kTrustedParserOptions:
      return FragmentParserOptions(options->GetAsTrustedParserOptions());
  }
}

// static
FragmentParserConfig FragmentParserConfig::ForContainer(
    ContainerNode* context,
    Sanitizer::Mode mode,
    const AtomicString& interface_name,
    const AtomicString& property_name) {
  CHECK(context->IsElementNode() || context->IsShadowRoot());
  return {
      .sanitizer_mode = mode,
      .parse_declarative_shadows =
          FragmentParserConfig::ParseDeclarativeShadowRoots::kParse,
      .force_html = FragmentParserConfig::ForceHtml::kForce,
      .interface_name = interface_name,
      .property_name = property_name,
      .context_element = context->IsElementNode()
                             ? To<Element>(context)
                             : &To<ShadowRoot>(context)->host(),
      .registry =
          context->IsElementNode()
              ? (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()
                     ? To<Element>(context)->customElementRegistry()
                     : nullptr)
              : To<ShadowRoot>(context)->customElementRegistry()};
}

DocumentFragment* ParseHTMLFragment(const String& markup,
                                    const FragmentParserConfig& config,
                                    FragmentParserOptions options,
                                    ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (RuntimeEnabledFeatures::TrustedTypesCreateParserOptionsEnabled()) {
    auto trusted_options = TrustedTypesCheckForParserOptions(
        options, MarkupInsertionMode::kFragment,
        config.context_element->GetExecutionContext(), config.interface_name,
        config.property_name, exception_state);
    if (!trusted_options) {
      return nullptr;
    }
    options = *trusted_options;
  }

  const ParserContentPolicy content_policy =
      options.run_scripts() == FragmentParserOptions::RunScripts::kRunScripts
          ? kAllowScriptingContentAndDoNotMarkAlreadyStarted
          : kAllowScriptingContent;

  const bool should_sanitize =
      options.sanitizer_init() ||
      (config.sanitizer_mode == Sanitizer::Mode::kSafe);

  StreamingSanitizer* streaming_sanitizer = nullptr;
  if (should_sanitize && RuntimeEnabledFeatures::StreamingSanitizerEnabled()) {
    streaming_sanitizer = SanitizerAPI::CreateStreamingSanitizer(
        config.sanitizer_mode, options, exception_state);
  }

  if (streaming_sanitizer &&
      !SanitizerAPI::AllowMutatingRootElement(config.sanitizer_mode,
                                              config.context_element)) {
    return nullptr;
  }

  DocumentFragment* fragment = ParseHTMLFragmentInternal(
      markup, config.context_element, content_policy,
      config.parse_declarative_shadows, config.force_html,
      should_sanitize ? ForceInertTemplate::kForce
                      : ForceInertTemplate::kDontForce,
      config.registry, exception_state, streaming_sanitizer);

  if (fragment && should_sanitize &&
      (!streaming_sanitizer || !fragment->GetDocument().IsHTMLDocument())) {
    SanitizerAPI::SanitizeInternal(config.sanitizer_mode,
                                   config.context_element, fragment, options,
                                   exception_state);
  }

  if (exception_state.HadException()) {
    return nullptr;
  }
  return fragment;
}

DocumentFragment* CreateContextualFragment(const String& html,
                                           Element* element,
                                           ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    return nullptr;
  }

  // Use null registry to create fragment if the context element is a
  // template element as the container of the document fragment will be a
  // document fragment without browsing context.
  CustomElementRegistry* registry =
      (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled() &&
       IsA<HTMLTemplateElement>(element))
          ? nullptr
          : element->customElementRegistry();

  DocumentFragment* fragment = blink::ParseHTMLFragment(
      html,
      {
          .interface_name = trusted_types_names::kRange,
          .property_name = trusted_types_names::kCreateContextualFragment,
          .context_element = element,
          .registry = registry,
      },
      FragmentParserOptions(FragmentParserOptions::RunScripts::kRunScripts),
      exception_state);

  if (!fragment) {
    return nullptr;
  }

  // We need to pop <html> and <body> elements and remove <head> to
  // accommodate folks passing complete HTML documents to make the
  // child of an element.
  Node* next_node = nullptr;
  for (Node* child = fragment->firstChild(); child; child = next_node) {
    next_node = child->nextSibling();
    if (IsA<HTMLHtmlElement>(child) || IsA<HTMLHeadElement>(child) ||
        IsA<HTMLBodyElement>(child)) {
      auto* child_element = To<HTMLElement>(child);
      if (Node* first_child = child_element->firstChild()) {
        next_node = first_child;
      }
      RemoveElementPreservingChildren(fragment, child_element);
    }
  }

  return fragment;
}

}  // namespace blink
