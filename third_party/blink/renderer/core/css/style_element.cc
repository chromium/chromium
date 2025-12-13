/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/style_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/blocking_attribute.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/import_map_error.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {
bool IsCSS(const AtomicString& type) {
  return type.empty() || EqualIgnoringASCIICase(type, keywords::kTextCss);
}

bool IsCSSModule(const AtomicString& type) {
  return EqualIgnoringASCIICase(type, keywords::kModule);
}
}  // namespace

StyleElement::StyleElement(Document* document, bool created_by_parser)
    : has_finished_parsing_children_(!created_by_parser),
      loading_(false),
      registered_as_candidate_(false),
      created_by_parser_(created_by_parser),
      start_position_(TextPosition::BelowRangePosition()),
      pending_sheet_type_(PendingSheetType::kNone),
      render_blocking_behavior_(RenderBlockingBehavior::kUnset) {
  if (created_by_parser && document &&
      document->GetScriptableDocumentParser() &&
      !document->IsInDocumentWrite()) {
    start_position_ =
        document->GetScriptableDocumentParser()->GetTextPosition();
  }
}

StyleElement::~StyleElement() = default;

StyleElement::ProcessingResult StyleElement::ProcessStyleSheet(
    Document& document,
    Element& element) {
  TRACE_EVENT0("blink", "StyleElement::processStyleSheet");
  DCHECK(element.isConnected());

  // Module type is static based upon when it's first connected.
  // TODO(crbug.com/448174611): Confirm this with the WHATWG and update behavior
  // according to WHATWG resolutions.
  if (RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled()) {
    if ((element_type_ == StyleType::kPending) && element.isConnected()) {
      // TODO(crbug.com/448174611): For consistency with Import Maps, should we
      // mimic passing "Already Started" state when cloneNode is called? This
      // would involve passing `element_type_` to the clone.
      if (IsCSSModule(this->type())) {
        element_type_ = StyleType::kModule;
      } else {
        element_type_ = StyleType::kClassic;
      }
    }

    // Sheet should always be empty for modules.
    DCHECK(!IsModule() || !sheet_);
  }
  // Classic <style> tags may have an associated stylesheet and need to added as
  // a candidate node.
  if (!IsModule()) {
    DCHECK(!sheet_);
    registered_as_candidate_ = true;
    document.GetStyleEngine().AddStyleSheetCandidateNode(element);
  }
  if (!has_finished_parsing_children_) {
    return kProcessingSuccessful;
  }
  return Process(element);
}

void StyleElement::RemovedFrom(Element& element,
                               ContainerNode& insertion_point) {
  if (!insertion_point.isConnected()) {
    return;
  }

  Document& document = element.GetDocument();
  if (registered_as_candidate_) {
    document.GetStyleEngine().RemoveStyleSheetCandidateNode(element,
                                                            insertion_point);
    registered_as_candidate_ = false;
  }

  if (sheet_) {
    ClearSheet(element);
  }
}

StyleElement::ProcessingResult StyleElement::ChildrenChanged(Element& element) {
  if (!has_finished_parsing_children_) {
    return kProcessingSuccessful;
  }
  probe::WillChangeStyleElement(&element);
  return Process(element);
}

StyleElement::ProcessingResult StyleElement::FinishParsingChildren(
    Element& element) {
  ProcessingResult result = Process(element);
  has_finished_parsing_children_ = true;
  return result;
}

StyleElement::ProcessingResult StyleElement::Process(Element& element) {
  if (!element.isConnected()) {
    return kProcessingSuccessful;
  }
  return CreateSheetOrModule(element, element.TextFromChildren());
}

void StyleElement::ClearSheet(Element& owner_element) {
  DCHECK(sheet_);

  if (sheet_->IsLoading()) {
    DCHECK(IsSameObject(owner_element));
    if (pending_sheet_type_ != PendingSheetType::kNonBlocking) {
      owner_element.GetDocument().GetStyleEngine().RemovePendingBlockingSheet(
          owner_element, pending_sheet_type_);
    }
    pending_sheet_type_ = PendingSheetType::kNone;
  }

  sheet_.Release()->ClearOwnerNode();
}

static bool IsInUserAgentShadowDOM(const Element& element) {
  ShadowRoot* root = element.ContainingShadowRoot();
  return root && root->IsUserAgent();
}

StyleElement::ProcessingResult StyleElement::CreateSheetOrModule(
    Element& element,
    const String& text) {
  DCHECK(element.isConnected());
  DCHECK(IsSameObject(element));
  Document& document = element.GetDocument();

  ContentSecurityPolicy* csp =
      element.GetExecutionContext()
          ? element.GetExecutionContext()
                ->GetContentSecurityPolicyForCurrentWorld()
          : nullptr;

  // CSP is bypassed for style elements in user agent shadow DOM.
  const bool passes_content_security_policy_checks =
      IsInUserAgentShadowDOM(element) ||
      (csp && csp->AllowInline(ContentSecurityPolicy::InlineType::kStyle,
                               &element, text, element.nonce(), document.Url(),
                               start_position_.line_));

  // TODO(crbug.com/448174611) - should Declarative CSS Modules continue
  // respecting CSP? Need to confirm with WHATWG.
  if (passes_content_security_policy_checks && IsModule()) {
    CHECK(RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled());
    AddImportMapEntry(element, text);

    // Return early, since we explicitly *don't* want to create a CSSStyleSheet
    // for CSS modules.
    return kProcessingSuccessful;
  }

  // Use a strong reference to keep the cache entry (which is a weak reference)
  // alive after ClearSheet().
  Persistent<CSSStyleSheet> old_sheet = sheet_;
  if (old_sheet) {
    ClearSheet(element);
  }

  CSSStyleSheet* new_sheet = nullptr;

  // If type is empty or CSS, this is a CSS style sheet.
  const AtomicString& type = this->type();
  if (IsCSS(type) && passes_content_security_policy_checks) {
    MediaQuerySet* media_queries = nullptr;
    const AtomicString& media_string = media();
    bool media_query_matches = true;
    if (!media_string.empty()) {
      media_queries =
          MediaQuerySet::Create(media_string, element.GetExecutionContext());
      if (LocalFrame* frame = document.GetFrame()) {
        MediaQueryEvaluator* evaluator =
            MakeGarbageCollected<MediaQueryEvaluator>(frame);
        media_query_matches = evaluator->Eval(*media_queries);
      }
    }
    auto type_and_behavior = ComputePendingSheetTypeAndRenderBlockingBehavior(
        element, media_query_matches, created_by_parser_);
    pending_sheet_type_ = type_and_behavior.first;
    render_blocking_behavior_ = type_and_behavior.second;

    loading_ = true;
    TextPosition start_position =
        start_position_ == TextPosition::BelowRangePosition()
            ? TextPosition::MinimumPosition()
            : start_position_;
    new_sheet = document.GetStyleEngine().CreateSheet(
        element, text, start_position, pending_sheet_type_,
        render_blocking_behavior_);
    new_sheet->SetMediaQueries(media_queries);
    loading_ = false;
  }

  sheet_ = new_sheet;
  if (sheet_) {
    sheet_->Contents()->CheckLoaded();
  }

  return passes_content_security_policy_checks ? kProcessingSuccessful
                                               : kProcessingFatalError;
}

void StyleElement::AddImportMapEntry(Element& element, const String& text) {
  CHECK(!sheet_);

  // Return early if there is no specifier
  // TODO(crbug.com/448174611) - Is this the correct behavior?
  if (!element.hasAttribute(html_names::kSpecifierAttr)) {
    return;
  }

  // Create an Import Map JSON string in the following format:
  // "imports": {
  //   "<specifier attribute value>": "<generated URL>"
  // }
  //
  // ...where <generated URL> can be either a Blob or a dataURI, depending on
  // whether features::kDeclarativeCSSModulesUseDataURI is set.
  // TODO(crbug.com/448174611) - finalize which approach to use with the WHATWG
  // and remove the other option.
  // TODO(crbug.com/448174611) - add links to each step from the spec once the
  // PR is merged.
  // TODO(crbug.com/364917757) - Use PendingImportMap here to reduce code (if
  // the dependency on the <script> element can be removed from
  // PendingImportMap).
  String url_string;
  ExecutionContext* context = element.GetExecutionContext();
  const bool use_data_uri =
      base::FeatureList::IsEnabled(features::kDeclarativeCSSModulesUseDataURI);
  if (use_data_uri) {
    // TODO(crbug.com/448174611) - consider encoding in base64 to decrease
    // string size in memory (at the expense of decoding on the CPU).
    url_string = StrCat({"data:text/css,", EncodeWithURLEscapeSequences(text)});
  } else {
    StringUtf8Adaptor utf8(text, Utf8ConversionMode::kLenient);
    auto* blob = Blob::Create(base::as_byte_span(utf8), "text/css");
    CHECK(blob);
    url_string = DOMURL::CreatePublicURL(context, blob);
  }
  KURL url(url_string);
  CHECK(url.IsValid());

  // The inner JSON object needs to be on the heap because
  // JSONObject::SetObject only accepts a unique_ptr.
  auto import_map_inner_json = std::make_unique<JSONObject>();
  import_map_inner_json->SetString(
      element.getAttribute(html_names::kSpecifierAttr), url_string);

  JSONObject import_map_outer_json;
  import_map_outer_json.SetObject("imports", std::move(import_map_inner_json));

  std::optional<ImportMapError> error_to_rethrow;
  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  // Even though ImportMap is garbage collected (and thus managed by Oilpan), we
  // don't need to store it as a Member because MergeExistingAndNewImportMaps
  // will copy-by-value the local import map strings into the global import map.
  ImportMap* import_map = ImportMap::Parse(import_map_outer_json.ToJSONString(),
                                           element.GetDocument().BaseURL(),
                                           *context, &error_to_rethrow);
  CHECK(import_map);
  CHECK(!error_to_rethrow.has_value());

  Modulator* modulator = Modulator::From(
      ToScriptStateForMainWorld(To<LocalDOMWindow>(context)->GetFrame()));
  modulator->MergeExistingAndNewImportMaps(import_map);

  // For Blob URL's, create a CSS module script and add it to the module map so
  // it can be accessed immediately. We don't need to do this for DataURI's
  // because they must be fetched later, so setting it now wouldn't accomplish
  // anything.
  // TODO(crbug.com/448174611) - add this part to the spec and add links to spec
  // here.
  // TODO(crbug.com/448174611) - should this be checking
  // (!module_script->HasParseError() && !module_script->HasErrorToRethrow())
  // before inserting into the module map? I don't see these being set for
  // invalid CSS or @import statements, but it seems like they should.
  if (!use_data_uri) {
    ModuleScriptCreationParams params(
        /*source_url=*/url, /*base_url=*/url, ScriptSourceLocationType::kInline,
        ResolvedModuleType::kCSS, ParkableString(text.Impl()),
        /*cache_handler=*/nullptr, network::mojom::ReferrerPolicy::kDefault,
        /*source_map_url=*/String());
    ValueWrapperSyntheticModuleScript* module_script =
        ValueWrapperSyntheticModuleScript::
            CreateCSSWrapperSyntheticModuleScript(params, modulator);
    CHECK(module_script);
    modulator->AddEntryToModuleMap(url, ModuleType::kCSS, module_script);
  }
}

bool StyleElement::IsLoading() const {
  DCHECK(!IsModule());
  if (loading_) {
    return true;
  }
  return sheet_ && sheet_->IsLoading();
}

bool StyleElement::IsModule() const {
  // It's only possible to set the type to module when the flag is enabled.
  DCHECK(element_type_ != StyleType::kModule ||
         RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled());
  return element_type_ == StyleType::kModule;
}

bool StyleElement::SheetLoaded(Document& document) {
  DCHECK(!IsModule());
  if (IsLoading()) {
    return false;
  }

  DCHECK(IsSameObject(*sheet_->ownerNode()));
  if (pending_sheet_type_ != PendingSheetType::kNonBlocking) {
    document.GetStyleEngine().RemovePendingBlockingSheet(*sheet_->ownerNode(),
                                                         pending_sheet_type_);
  }
  document.GetStyleEngine().SetNeedsActiveStyleUpdate(
      sheet_->ownerNode()->GetTreeScope());
  pending_sheet_type_ = PendingSheetType::kNone;
  return true;
}

void StyleElement::SetToPendingState(Document& document, Element& element) {
  DCHECK(!IsModule());
  DCHECK(IsSameObject(element));
  DCHECK_LT(pending_sheet_type_, PendingSheetType::kBlocking);
  pending_sheet_type_ = PendingSheetType::kBlocking;
  document.GetStyleEngine().AddPendingBlockingSheet(element,
                                                    pending_sheet_type_);
}

void StyleElement::BlockingAttributeChanged(Element& element) {
  // If this is a dynamically inserted style element, and the `blocking`
  // has changed so that the element is no longer render-blocking, then unblock
  // rendering on this element. Note that Parser-inserted stylesheets are
  // render-blocking by default, so removing `blocking=render` does not unblock
  // rendering.
  if (pending_sheet_type_ != PendingSheetType::kDynamicRenderBlocking) {
    return;
  }
  if (const auto* html_element = DynamicTo<HTMLElement>(element);
      !html_element || html_element->IsPotentiallyRenderBlocking()) {
    return;
  }
  element.GetDocument().GetStyleEngine().RemovePendingBlockingSheet(
      element, pending_sheet_type_);
  pending_sheet_type_ = PendingSheetType::kNonBlocking;
}

void StyleElement::Trace(Visitor* visitor) const {
  visitor->Trace(sheet_);
}

}  // namespace blink
