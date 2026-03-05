/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/processing_instruction.h"

#include <memory>

#include "third_party/blink/renderer/core/css/counters_attachment_context.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/editing/serializers/markup_formatter.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/xsl_style_sheet_resource.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"  // for parseAttributes()
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_rs.h"  // for parseAttributesRust()
#include "third_party/blink/renderer/core/xml/xsl_style_sheet.h"
#include "third_party/blink/renderer/core/xml/xslt_processor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool IsLocalSheet(const String& href) {
  return href.length() > 1 && href[0] == '#';
}

}  // namespace

ProcessingInstruction::ProcessingInstruction(Document& document,
                                             const String& target,
                                             const String& data)
    : CharacterData(document, data, kCreateProcessingInstruction),
      target_(target),
      loading_(false),
      alternate_(false),
      is_css_(false),
      is_xsl_(false),
      listener_for_xslt_(nullptr) {}

ProcessingInstruction::~ProcessingInstruction() = default;

bool ProcessingInstruction::IsXSL() const {
  CHECK(!is_xsl_ || RuntimeEnabledFeatures::XSLTEnabled());
  return is_xsl_;
}

EventListener* ProcessingInstruction::EventListenerForXSLT() {
  if (!listener_for_xslt_)
    return nullptr;

  return listener_for_xslt_->ToEventListener();
}

void ProcessingInstruction::ClearEventListenerForXSLT() {
  if (listener_for_xslt_) {
    listener_for_xslt_->Detach();
    listener_for_xslt_.Clear();
  }
}

String ProcessingInstruction::nodeName() const {
  return target_;
}

CharacterData* ProcessingInstruction::CloneWithData(Document& factory,
                                                    const String& data) const {
  // FIXME: Is it a problem that this does not copy local_href_?
  // What about other data members?
  return MakeGarbageCollected<ProcessingInstruction>(factory, target_, data);
}

void ProcessingInstruction::DidChangeData() {
  attributes_dirty_ = true;
  UpdateStylesheetIfNeeded();
}

void ProcessingInstruction::UpdateStylesheetIfNeeded() {
  if (!IsXMLStylesheet()) {
    return;
  }

  if (sheet_) {
    if (sheet_->IsLoading()) {
      RemovePendingSheet();
    }
    ClearSheet();
  }

  String href;
  String charset;
  if (CheckStyleSheet(href, charset)) {
    ProcessStylesheet(href, charset);
  }
}

void ProcessingInstruction::ProcessAttributesIfNeeded() {
  if (!attributes_dirty_) {
    return;
  }

  attributes_dirty_ = false;
  attributes_.clear();

  if (GetDocument().IsXMLDocument() ||
      (IsXMLStylesheet() &&
       !RuntimeEnabledFeatures::HTMLProcessingInstructionEnabled())) {
    // see http://www.w3.org/TR/xml-stylesheet/
    // ### support stylesheet included in a fragment of this (or another)
    // document
    // ### make sure this gets called when adding from javascript
    bool attrs_ok;
    HashMap<String, String> attrs;
    if (RuntimeEnabledFeatures::XMLParsingRustEnabled()) {
      attrs = ParseAttributesRust(data_, attrs_ok);
    } else {
      attrs = ParseAttributes(data_, attrs_ok);
    }
    if (!attrs_ok) {
      return;
    }

    for (const auto& pair : attrs) {
      attributes_.push_back(
          KeyValuePair<AtomicString, AtomicString>(pair.key, pair.value));
    }
    return;
  }

  CHECK(GetDocument().IsHTMLDocument());

  StringBuilder fake_html;
  fake_html.Append("<attrs ");
  fake_html.Append(data_);
  fake_html.Append("></attrs>");
  const DocumentFragment* fragment = CreateFragmentFromMarkup(
      GetDocument(), fake_html.ToString(), GetDocument().BaseURL(),
      ParserContentPolicy::kDisallowScriptingAndPluginContent);
  const Node* first = fragment->firstChild();
  if (!first || !first->IsElementNode()) {
    return;
  }

  const Element& fake_element = To<Element>(*first);

  CHECK_EQ(fake_element.localName(), "attrs");

  for (const auto& attribute : fake_element.Attributes()) {
    attributes_.push_back(KeyValuePair<AtomicString, AtomicString>(
        attribute.LocalName(), attribute.Value()));
  }
}

AtomicString ProcessingInstruction::LowercaseIfNeeded(
    const AtomicString& name) const {
  return GetDocument().IsHTMLDocument() ? name.LowerASCII() : name;
}

bool ProcessingInstruction::ValidateAttributeName(
    const AtomicString& name,
    ExceptionState& exception_state) const {
  if (!Document::IsValidAttributeLocalName(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                      "Invalid attribute name: " + name);
    return false;
  }
  return true;
}

const AtomicString& ProcessingInstruction::GetAttributeValue(
    const AtomicString& name,
    const AtomicString& default_value) {
  DCHECK_EQ(name, LowercaseIfNeeded(name));
  ProcessAttributesIfNeeded();
  for (const auto& pair : attributes_) {
    if (pair.key == name) {
      return pair.value;
    }
  }
  return default_value;
}

bool ProcessingInstruction::HasAttribute(const AtomicString& name) {
  DCHECK_EQ(name, LowercaseIfNeeded(name));
  ProcessAttributesIfNeeded();
  for (const auto& pair : attributes_) {
    if (pair.key == name) {
      return true;
    }
  }
  return false;
}

void ProcessingInstruction::SetAttribute(const AtomicString& name,
                                         const AtomicString& value) {
  DCHECK_EQ(name, LowercaseIfNeeded(name));
  DCHECK(ValidateAttributeName(name, ASSERT_NO_EXCEPTION));

  ProcessAttributesIfNeeded();
  for (auto& pair : attributes_) {
    if (pair.key == name) {
      pair.value = value;
      UpdateDataFromAttributes();
      return;
    }
  }
  attributes_.push_back(KeyValuePair<AtomicString, AtomicString>(name, value));
  UpdateDataFromAttributes();
}

void ProcessingInstruction::RemoveAttribute(const AtomicString& name) {
  DCHECK_EQ(name, LowercaseIfNeeded(name));
  ProcessAttributesIfNeeded();
  const wtf_size_t size = attributes_.size();
  for (wtf_size_t i = 0; i < size; ++i) {
    if (attributes_[i].key == name) {
      attributes_.EraseAt(i);
      UpdateDataFromAttributes();
      return;
    }
  }
}

void ProcessingInstruction::ToggleAttribute(const AtomicString& name,
                                            std::optional<bool> force,
                                            ExceptionState& exception_state) {
  if (!ValidateAttributeName(name, exception_state)) {
    return;
  }

  DCHECK_EQ(name, LowercaseIfNeeded(name));
  const bool already_there = HasAttribute(name);
  force = force.value_or(!already_there);

  if (*force) {
    if (!already_there) {
      SetAttribute(name, g_empty_atom);
    }
  } else {
    RemoveAttribute(name);
  }
}

bool ProcessingInstruction::hasAttributes() {
  ProcessAttributesIfNeeded();
  return !attributes_.empty();
}

Vector<AtomicString> ProcessingInstruction::getAttributeNames() {
  ProcessAttributesIfNeeded();
  Vector<AtomicString> names;
  names.reserve(attributes_.size());
  for (const auto& pair : attributes_) {
    names.push_back(pair.key);
  }
  return names;
}

void ProcessingInstruction::UpdateDataFromAttributes() {
  StringBuilder builder;
  const wtf_size_t size = attributes_.size();
  for (wtf_size_t i = 0; i < size; ++i) {
    if (i) {
      builder.Append(" ");
    }
    builder.Append(attributes_[i].key);
    builder.Append("=\"");
    MarkupFormatter::AppendAttributeValue(builder, attributes_[i].value,
                                          GetDocument().IsHTMLDocument());
    builder.Append("\"");
  }
  SetDataFromAttributeChange(builder.ReleaseString());
  UpdateStylesheetIfNeeded();
}

bool ProcessingInstruction::IsXMLStylesheet() const {
  return (target_ == "xml-stylesheet") && GetDocument().GetFrame() &&
         (parentNode() == GetDocument());
}

bool ProcessingInstruction::CheckStyleSheet(String& href, String& charset) {
  if (!IsXMLStylesheet()) {
    return false;
  }

  ProcessAttributesIfNeeded();

  DEFINE_STATIC_LOCAL(AtomicString, kType, ("type"));
  DEFINE_STATIC_LOCAL(AtomicString, kHref, ("href"));
  DEFINE_STATIC_LOCAL(AtomicString, kCharset, ("charset"));
  DEFINE_STATIC_LOCAL(AtomicString, kAlternate, ("alternate"));
  DEFINE_STATIC_LOCAL(AtomicString, kTitle, ("title"));
  DEFINE_STATIC_LOCAL(AtomicString, kMedia, ("media"));

  AtomicString type = GetAttributeValue(kType, g_empty_atom);
  is_css_ = type.empty() || type == "text/css";
  is_xsl_ = (type == "text/xml" || type == "text/xsl" ||
             type == "application/xml" || type == "application/xhtml+xml" ||
             type == "application/rss+xml" || type == "application/atom+xml");
  if (!is_css_ && !is_xsl_)
    return false;

  if (is_xsl_ && !RuntimeEnabledFeatures::XSLTEnabled()) {
    XSLTProcessor::ReportXSLTDisabled(GetDocument(),
                                      /*exception_state*/ nullptr);
    is_xsl_ = false;
    return false;
  }

  href = GetAttributeValue(kHref, g_empty_atom);

  // Disallow "external" XSLT stylesheets in SVG documents in image contexts.
  if (is_xsl_ && SVGImage::IsInSVGImage(this) && !IsLocalSheet(href)) {
    is_xsl_ = false;
    return false;
  }

  if (is_xsl_ && GetDocument().IsSVGDocument()) {
    if (SVGImage::IsInSVGImage(this)) {
      // Encountering XSL inside an external SVG image can't be counted through
      // the document we retrieve through GetDocument() here, as that is the SVG
      // document of the image. Instead, we set the flag on the SVG image, and
      // in SVGImage::UpdateUseCountersAfterLoad() send the use counter as part
      // of the metrics of the embedding document, not the SVG document of the
      // image.
      if (Page* page = GetDocument().GetPage()) {
        if (auto* client =
                DynamicTo<IsolatedSVGChromeClient>(&page->GetChromeClient())) {
          client->SetDidEncounterXSL();
        }
      }
    } else {
      UseCounter::Count(GetDocument(), WebFeature::kXSLPIInSVGStandaloneDoc);
    }
  }

  charset = GetAttributeValue(kCharset, g_empty_atom);
  alternate_ = GetAttributeValue(kAlternate, g_empty_atom) == "yes";
  title_ = GetAttributeValue(kTitle, g_empty_atom);
  media_ = GetAttributeValue(kMedia, g_empty_atom);

  return !alternate_ || !title_.empty();
}

void ProcessingInstruction::ProcessStylesheet(const String& href,
                                              const String& charset) {
  CHECK(IsXMLStylesheet());
  if (IsLocalSheet(href)) {
    local_href_ = href.Substring(1);
    // We need to make a synthetic XSLStyleSheet that is embedded.
    // It needs to be able to kick off import/include loads that
    // can hang off some parent sheet.
    if (is_xsl_) {
      KURL final_url(local_href_);
      sheet_ = MakeGarbageCollected<XSLStyleSheet>(this, final_url.GetString(),
                                                   final_url, true);
      loading_ = false;
    }
    return;
  }

  ClearResource();

  ResourceLoaderOptions options(GetExecutionContext()->GetCurrentWorld());
  options.initiator_info.name =
      fetch_initiator_type_names::kProcessinginstruction;
  FetchParameters params(ResourceRequest(GetDocument().CompleteURL(href)),
                         options);
  loading_ = true;
  if (is_xsl_) {
    params.MutableResourceRequest().SetMode(
        network::mojom::RequestMode::kSameOrigin);
    XSLStyleSheetResource::Fetch(params, GetDocument().Fetcher(), this);
  } else {
    params.SetCharset(charset.empty() ? GetDocument().Encoding()
                                      : TextEncoding(charset));
    GetDocument().GetStyleEngine().AddPendingBlockingSheet(
        *this, PendingSheetType::kBlocking);
    CSSStyleSheetResource::Fetch(params, GetDocument().Fetcher(), this);
  }
}

bool ProcessingInstruction::IsLoading() const {
  if (loading_)
    return true;
  if (!sheet_)
    return false;
  return sheet_->IsLoading();
}

bool ProcessingInstruction::SheetLoaded() {
  if (!IsLoading()) {
    if (!DocumentXSLT::SheetLoaded(GetDocument(), this))
      RemovePendingSheet();
    return true;
  }
  return false;
}

void ProcessingInstruction::NotifyFinished(Resource* resource) {
  if (!isConnected()) {
    DCHECK(!sheet_);
    return;
  }

  std::unique_ptr<IncrementLoadEventDelayCount> delay =
      is_xsl_ ? std::make_unique<IncrementLoadEventDelayCount>(GetDocument())
              : nullptr;
  if (is_xsl_) {
    sheet_ = MakeGarbageCollected<XSLStyleSheet>(
        this, resource->Url(), resource->GetResponse().ResponseUrl(), false);
    To<XSLStyleSheet>(sheet_.Get())
        ->ParseString(To<XSLStyleSheetResource>(resource)->Sheet());
  } else {
    DCHECK(is_css_);
    auto* style_resource = To<CSSStyleSheetResource>(resource);
    auto* parser_context = MakeGarbageCollected<CSSParserContext>(
        GetDocument(), style_resource->GetResponse().ResponseUrl(),
        style_resource->GetResponse().IsCorsSameOrigin(),
        Referrer(style_resource->GetResponse().ResponseUrl(),
                 style_resource->GetReferrerPolicy()),
        style_resource->Encoding());
    if (style_resource->GetResourceRequest().IsAdResource())
      parser_context->SetIsAdRelated();

    auto* new_sheet = MakeGarbageCollected<StyleSheetContents>(
        parser_context, style_resource->Url());

    auto* css_sheet = MakeGarbageCollected<CSSStyleSheet>(new_sheet, *this);
    css_sheet->setDisabled(alternate_);
    css_sheet->SetTitle(title_);
    if (!alternate_ && !title_.empty()) {
      GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(
          title_);
    }
    css_sheet->SetMediaQueries(
        MediaQuerySet::Create(media_, GetExecutionContext()));
    sheet_ = css_sheet;
    // We don't need the cross-origin security check here because we are
    // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
    // type.
    css_sheet->Contents()->ParseString(
        style_resource->SheetText(parser_context));
  }

  ClearResource();
  loading_ = false;

  if (is_css_)
    To<CSSStyleSheet>(sheet_.Get())->Contents()->CheckLoaded();
  else if (is_xsl_)
    To<XSLStyleSheet>(sheet_.Get())->CheckLoaded();
}

Node::InsertionNotificationRequest ProcessingInstruction::InsertedInto(
    ContainerNode& insertion_point) {
  CharacterData::InsertedInto(insertion_point);
  if (!insertion_point.isConnected()) {
    return kInsertionDone;
  }
  return Node::kInsertionShouldCallDidNotifySubtreeInsertions;
}

void ProcessingInstruction::DidNotifySubtreeInsertionsToDocument() {
  CharacterData::DidNotifySubtreeInsertionsToDocument();

  String href;
  String charset;
  bool is_valid = CheckStyleSheet(href, charset);
  if (!DocumentXSLT::ProcessingInstructionInsertedIntoDocument(GetDocument(),
                                                               this))
    GetDocument().GetStyleEngine().AddStyleSheetCandidateNode(*this);
  if (is_valid)
    ProcessStylesheet(href, charset);
}

void ProcessingInstruction::RemovedFrom(ContainerNode& insertion_point) {
  CharacterData::RemovedFrom(insertion_point);
  if (!insertion_point.isConnected())
    return;

  // No need to remove XSLStyleSheet from StyleEngine.
  if (!DocumentXSLT::ProcessingInstructionRemovedFromDocument(GetDocument(),
                                                              this)) {
    GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(
        *this, insertion_point);
  }

  if (IsLoading())
    RemovePendingSheet();

  if (sheet_) {
    DCHECK_EQ(sheet_->ownerNode(), this);
    ClearSheet();
  }

  // No need to remove pending sheets.
  ClearResource();
}

void ProcessingInstruction::ClearSheet() {
  DCHECK(sheet_);
  sheet_.Release()->ClearOwnerNode();
}

void ProcessingInstruction::RemovePendingSheet() {
  if (is_xsl_)
    return;
  GetDocument().GetStyleEngine().RemovePendingBlockingSheet(
      *this, PendingSheetType::kBlocking);
}

void ProcessingInstruction::Trace(Visitor* visitor) const {
  visitor->Trace(sheet_);
  visitor->Trace(listener_for_xslt_);
  CharacterData::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
