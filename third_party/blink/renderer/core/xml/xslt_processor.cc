/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
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

#include "third_party/blink/renderer/core/xml/xslt_processor.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_encoding_data.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/transform_source.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

static inline void TransformTextStringToXHTMLDocumentString(String& text) {
  // Modify the output so that it is a well-formed XHTML document with a <pre>
  // tag enclosing the text.
  text.Replace('&', "&amp;");
  text.Replace('<', "&lt;");
  text =
      StrCat({"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
              "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
              "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
              "<head><title/></head>\n"
              "<body>\n"
              "<pre>",
              text,
              "</pre>\n"
              "</body>\n"
              "</html>\n"});
}

namespace {
void AddXSLTConsoleWarning(Document& document, const String& message) {
  if (auto* window = document.domWindow()) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
                                  ConsoleMessage::Source::kDeprecation,
                                  ConsoleMessage::Level::kWarning, message),
                              /*discard_duplicates=*/true);
  }
}
}  // namespace

bool XSLTProcessor::IsXSLTEnabled(const ExecutionContext* context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kXSLTEnabledPolicy)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
               blink::switches::kXSLTEnabledPolicy) == "true";
  }
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (window->document() && window->document()->IsCAPAlert() &&
        RuntimeEnabledFeatures::EnableXSLTForCAPAlertsEnabled(context)) {
      return true;
    }
  }
  return RuntimeEnabledFeatures::XSLTEnabled(context);
}

void XSLTProcessor::ReportXSLTDisabled(Document& document,
                                       ExceptionState* exception_state) {
  CHECK(!IsXSLTEnabled(document.GetExecutionContext()));
  if (RuntimeEnabledFeatures::XSLTSpecialTrialEnabled()) {
    // Special trial run of XSLT removal (pre-stable channels, via Finch).
    AddXSLTConsoleWarning(
        document,
        "Usage of XSLTProcessor or XSLT Processing Instructions was detected. "
        "These features have been deprecated by all browsers, and a special "
        "early trial of complete removal is underway in this browser.\n"
        "--> If you are a *user* experiencing a problem, please report the "
        "issue directly to the operator of the website.\n"
        "--> If you are a site owner, and you think this trial is causing an "
        "unexpected issue, please report a bug at "
        "https://issues.chromium.org/issues/"
        "new?component=1456730&template=2210866");
  } else {
    // Normal case - XSLT is disabled.
    AddXSLTConsoleWarning(
        document,
        "XSLTProcessor and XSLT Processing Instructions have been "
        "removed in this browser. See "
        "https://chromestatus.com/feature/4709671889534976.");
  }
  if (exception_state) {
    exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                       "XSLT is disabled");
  }
}

XSLTProcessor::XSLTProcessor(PassKey,
                             Document& document,
                             WebFeature feature,
                             ExceptionState& exception_state)
    : document_(&document) {
  if (!IsXSLTEnabled(document.GetExecutionContext())) {
    // Ordinarily we will not get here, since in this case the runtime enabled
    // feature will be disabled, which removes the XSLTProcessor from IDL.
    // However, there are corner cases, such as that Finch has disabled XSLT
    // via the base::Feature, but the user has explicitly set the runtime
    // enabled feature back to true with `--enable-blink-features`.
    ReportXSLTDisabled(document, &exception_state);
    return;
  }
  // XSLT is still enabled. Use count, report the deprecation, and add an
  // explicit console message here for visibility, due to crbug.com/40069336.
  document.CountDeprecation(feature);
  AddXSLTConsoleWarning(
      document,
      "XSLTProcessor and XSLT Processing Instructions have been "
      "deprecated by all browsers. These features will be removed from "
      "this browser soon. See "
      "https://chromestatus.com/feature/4709671889534976.");
}

XSLTProcessor::~XSLTProcessor() = default;

namespace {

// The banner lives in the transformed document, whose stylesheets are not
// under our control, so everything is styled inline.
constexpr char kBannerStyle[] =
    "display: block; background-color: #d9534f; color: white; "
    "padding: 12px 44px; margin-bottom: 20px; font-size: 16px; "
    "font-weight: bold; text-align: center; font-family: sans-serif; "
    "position: relative; z-index: 2147483647; line-height: normal;";
constexpr char kLinkStyle[] = "color: white; text-decoration: underline;";
constexpr char kCloseButtonStyle[] =
    "position: absolute; top: 6px; right: 8px; background: transparent; "
    "border: none; color: inherit; font: inherit; font-size: 20px; "
    "line-height: 1; padding: 4px 8px; cursor: pointer;";
constexpr char kDismissLabelStyle[] =
    "display: block; margin-top: 8px; font-size: 14px; font-weight: normal; "
    "cursor: pointer;";
constexpr char kDismissCheckboxStyle[] =
    "vertical-align: middle; margin-right: 6px;";

// Tracks whether the "Never show this warning" checkbox was checked by the
// user. The banner lives in the page's own DOM, so page script can toggle the
// checkbox itself, either by dispatching a synthetic click or by setting
// `checked` directly. Neither should affect the persisted setting.
class BannerCheckboxListener final : public NativeEventListener {
 public:
  explicit BannerCheckboxListener(HTMLInputElement* checkbox)
      : checkbox_(checkbox) {}

  // Two independent signals have to agree: the user's own clicks must have
  // left the box checked, and the box must still be displaying as checked.
  // Neither is trustworthy alone. Script can assign to `checked` without
  // firing any event, so the displayed state can drift from what the user
  // did; and script can assign to `checked` from a capture-phase listener
  // that runs before Invoke() below, so the displayed state during dispatch
  // isn't necessarily the user's doing either. Requiring both means script
  // tampering can only ever keep the setting from being persisted, which is
  // harmless, and never force it on.
  bool CheckedByUser() const {
    return toggled_on_by_user_ && checkbox_->Checked();
  }

  void Invoke(ExecutionContext*, Event* event) override {
    if (!event || !event->isTrusted()) {
      return;
    }
    // Deliberately does not read `checked`: by the time this runs, a
    // capture-phase listener in the page may have changed it. A trusted
    // click toggles the checkbox, so just track the toggles. If the page
    // cancels the click, the toggle is reverted after dispatch and this
    // count goes out of step with the checkbox, but then the two signals
    // above disagree and the setting isn't persisted.
    toggled_on_by_user_ = !toggled_on_by_user_;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(checkbox_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<HTMLInputElement> checkbox_;
  bool toggled_on_by_user_ = false;
};

// Handles clicks on the banner's close button: removes the banner, and, if the
// user checked the "Never show this warning" checkbox, asks the browser to
// stop showing the banner on any site from now on.
class BannerCloseListener final : public NativeEventListener {
 public:
  BannerCloseListener(Element* banner, BannerCheckboxListener* checkbox_state)
      : banner_(banner), checkbox_state_(checkbox_state) {}

  void Invoke(ExecutionContext*, Event* event) override {
    // The banner lives in the page's own DOM, so page script can call
    // `close_button.click()` or dispatch a synthetic click. Only a real user
    // click may suppress the banner globally for this profile. Untrusted
    // clicks still dismiss the banner, which is harmless: script could just
    // as well remove the element itself.
    if (event && event->isTrusted() && checkbox_state_->CheckedByUser()) {
      // The frame is looked up through the banner rather than captured up
      // front, so that this still does the right thing if the transformed
      // document was adopted into a different frame.
      if (LocalFrame* frame = banner_->GetDocument().GetFrame()) {
        frame->GetLocalFrameHostRemote().SuppressXSLTDeprecationBanner();
      }
    }
    banner_->remove();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(banner_);
    visitor->Trace(checkbox_state_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<Element> banner_;
  Member<BannerCheckboxListener> checkbox_state_;
};

static Element* CreateBannerLink(Document& document,
                                 const String& href,
                                 const String& text) {
  Element* link = document.CreateRawElement(
      html_names::kATag, CreateElementFlags::ByCreateElement());
  link->setAttribute(html_names::kHrefAttr, AtomicString(href));
  link->setAttribute(html_names::kTargetAttr, AtomicString("_blank"));
  link->setAttribute(html_names::kRelAttr, AtomicString("noopener noreferrer"));
  link->setAttribute(html_names::kStyleAttr, AtomicString(kLinkStyle));
  link->appendChild(document.createTextNode(text));
  return link;
}

struct BannerLink {
  const char* href;
  String text;
};

// Appends `message` to `banner`, replacing its "$1" and "$2" placeholders with
// links to `link1` and `link2` respectively. The placeholders are expanded into
// elements rather than substituted as text, so the message is split here rather
// than by Locale::QueryString(). A translation may place the placeholders in
// any order, or leave one out.
static void AppendLocalizedBannerText(Document& document,
                                      ContainerNode* banner_container,
                                      const String& message,
                                      const BannerLink& link1,
                                      const BannerLink& link2) {
  wtf_size_t text_start = 0;
  for (int i = 0; i < 2; ++i) {
    wtf_size_t position1 = message.find("$1", text_start);
    wtf_size_t position2 = message.find("$2", text_start);
    wtf_size_t position = std::min(position1, position2);
    if (position == String::npos) {
      break;
    }
    if (position > text_start) {
      banner_container->appendChild(document.createTextNode(
          message.substr(text_start, position - text_start)));
    }
    const BannerLink& link = position == position1 ? link1 : link2;
    banner_container->appendChild(
        CreateBannerLink(document, link.href, link.text));
    text_start = position + 2;
  }
  if (text_start < message.length()) {
    banner_container->appendChild(
        document.createTextNode(message.substr(text_start)));
  }
}

// Appends the "Never show this warning" checkbox and the close button. The
// checkbox state is only acted on when the close button is clicked.
static void AppendDismissControls(Document& document,
                                  ContainerNode* banner_container,
                                  Element* banner) {
  auto* checkbox = To<HTMLInputElement>(document.CreateRawElement(
      html_names::kInputTag, CreateElementFlags::ByCreateElement()));
  checkbox->setAttribute(html_names::kTypeAttr, AtomicString("checkbox"));
  checkbox->setAttribute(html_names::kStyleAttr,
                         AtomicString(kDismissCheckboxStyle));
  auto* checkbox_state = MakeGarbageCollected<BannerCheckboxListener>(checkbox);
  checkbox->addEventListener(event_type_names::kClick, checkbox_state);

  Element* label = document.CreateRawElement(
      html_names::kLabelTag, CreateElementFlags::ByCreateElement());
  label->setAttribute(html_names::kStyleAttr, AtomicString(kDismissLabelStyle));
  label->appendChild(checkbox);
  label->appendChild(
      document.createTextNode(Locale::DefaultLocale().QueryString(
          IDS_XSLT_DEPRECATION_BANNER_NEVER_SHOW_AGAIN)));
  banner_container->appendChild(label);

  Element* close_button = document.CreateRawElement(
      html_names::kButtonTag, CreateElementFlags::ByCreateElement());
  close_button->setAttribute(html_names::kTypeAttr, AtomicString("button"));
  close_button->setAttribute(html_names::kStyleAttr,
                             AtomicString(kCloseButtonStyle));
  close_button->setAttribute(html_names::kAriaLabelAttr, AtomicString("Close"));
  close_button->setAttribute(html_names::kTitleAttr, AtomicString("Close"));
  // Content attributes survive cloning; addEventListener() listeners do not.
  // This keeps the close button working when a page clones the banner, e.g.
  // via importNode() on a transformToDocument() result. Note that this will not
  // pass CSP/trusted types, so it's not a perfect solution.
  close_button->setAttribute(html_names::kOnclickAttr,
                             AtomicString("this.getRootNode().host.remove();"));
  // U+00D7 MULTIPLICATION SIGN.
  close_button->appendChild(document.createTextNode(String(u"\u00D7")));
  close_button->addEventListener(
      event_type_names::kClick,
      MakeGarbageCollected<BannerCloseListener>(banner, checkbox_state));
  banner_container->appendChild(close_button);
}

template <typename Callback>
static void CreateAndAppendBanner(Document& document, Callback build_banner) {
  Element* target = document.body();
  if (!target) {
    target = document.documentElement();
  }
  if (!target) {
    return;
  }

  Element* banner = document.CreateRawElement(
      QualifiedName(g_null_atom, AtomicString("xslt-warning-banner"),
                    html_names::xhtmlNamespaceURI),
      CreateElementFlags::ByCreateElement());
  banner->SetCustomElementState(CustomElementState::kUndefined);
  banner->setAttribute(html_names::kStyleAttr, AtomicString(kBannerStyle));
  ShadowRoot& shadow_root = banner->AttachShadowRootInternal(
      ShadowRootMode::kOpen, FocusDelegation::kNone, SlotAssignmentMode::kNamed,
      CustomElementRegistryAssignment::Inherit(),
      /*serializable=*/false, /*clonable=*/true,
      /*reference_target=*/g_null_atom);
  build_banner(&shadow_root);
  AppendDismissControls(document, &shadow_root, banner);
  target->insertBefore(banner, target->firstChild());
}

constexpr char kXhtmlNamespace[] = "http://www.w3.org/1999/xhtml";

static bool IsXhtmlScriptWithSrc(xmlNodePtr node) {
  return node->type == XML_ELEMENT_NODE &&
         xmlStrEqual(node->name, BAD_CAST "script") && node->ns &&
         xmlStrEqual(node->ns->href, BAD_CAST kXhtmlNamespace) &&
         xmlHasNsProp(node, BAD_CAST "src", nullptr);
}

// Returns true if the XSLT source document contains an XHTML <script src=...>
// element at most two levels below the document element. Such a script never
// runs in a browser with native XSLT, because the parser stops at the
// xml-stylesheet processing instruction before the document element is even
// created. It will run once XSLT is removed, so its presence means the site
// has deliberately prepared for removal (typically by deploying an XSLT
// polyfill), and the deprecation banner would be a false alarm.
static bool SourceHasPolyfillScript(Document& owner_document) {
  TransformSource* transform_source = owner_document.GetTransformSource();
  if (!transform_source) {
    return false;
  }
  xmlDocPtr source = transform_source->PlatformSource();
  if (!source) {
    return false;
  }
  xmlNodePtr root = xmlDocGetRootElement(source);
  if (!root) {
    return false;
  }
  for (xmlNodePtr child = root->children; child; child = child->next) {
    if (IsXhtmlScriptWithSrc(child)) {
      return true;
    }
    for (xmlNodePtr grandchild = child->children; grandchild;
         grandchild = grandchild->next) {
      if (IsXhtmlScriptWithSrc(grandchild)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if the main world global object of `context` has a
// `createXSLTTransformModule` property. That function is installed by the XSLT
// polyfill, so its presence means the page can keep working without native
// XSLT, and the deprecation banner would be a false alarm.
static bool ContextHasPolyfillGlobal(ExecutionContext* context) {
  if (!context) {
    return false;
  }
  ScriptState* script_state = ToScriptStateForMainWorld(context);
  if (!script_state || !script_state->ContextIsValid()) {
    return false;
  }
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> v8_context = script_state->GetContext();
  // The lookup runs page script if the property has a getter, so swallow any
  // exception it throws.
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> value;
  if (!v8_context->Global()
           ->Get(v8_context,
                 V8AtomicString(isolate, "createXSLTTransformModule"))
           .ToLocal(&value)) {
    return false;
  }
  return !value->IsUndefined() && !value->IsNull();
}

// Document::GetSettings() returns null for a frameless document, which is what
// XSLTProcessor.transformToDocument() produces, so the caller can't use it.
// The execution context is the window that created the document, which does
// have a frame, and therefore settings.
static const Settings* SettingsForBanner(ExecutionContext* context) {
  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (!window) {
    return nullptr;
  }
  LocalFrame* frame = window->GetFrame();
  return frame ? frame->GetSettings() : nullptr;
}

static void InjectXSLTWarningBanner(bool is_cap_alert_xslt,
                                    bool source_has_polyfill_script,
                                    Document& document) {
  ExecutionContext* context = document.GetExecutionContext();
  if (!RuntimeEnabledFeatures::GenerateXSLTWarningBannerEnabled(context)) {
    return;
  }
  const Settings* settings = SettingsForBanner(context);
  if (settings && settings->GetXSLTDeprecationBannerSuppressed()) {
    // The user checked "Never show this warning" and closed a previous banner.
    return;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kXSLTEnabledPolicy) &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          blink::switches::kXSLTEnabledPolicy) == "true") {
    return;
  }
  if (context &&
      context->FeatureEnabled(mojom::blink::OriginTrialFeature::kXSLT)) {
    return;
  }
  if (source_has_polyfill_script || ContextHasPolyfillGlobal(context)) {
    return;
  }
  Locale& locale = Locale::DefaultLocale();
  const BannerLink extension_link = {
      "https://chromewebstore.google.com/search/XSLT%20Polyfill",
      locale.QueryString(IDS_XSLT_DEPRECATION_BANNER_EXTENSION_LINK)};
  if (is_cap_alert_xslt) {
    const BannerLink removal_link = {
        "https://chromestatus.com/feature/4709671889534976",
        locale.QueryString(IDS_XSLT_DEPRECATION_BANNER_CAP_ALERT_REMOVAL_LINK)};
    String message =
        locale.QueryString(IDS_XSLT_DEPRECATION_BANNER_CAP_ALERT_TEXT);
    CreateAndAppendBanner(document, [&](ContainerNode* banner) {
      AppendLocalizedBannerText(document, banner, message, removal_link,
                                extension_link);
    });
  } else {
    const BannerLink removal_link = {
        "https://chromestatus.com/feature/4709671889534976",
        locale.QueryString(IDS_XSLT_DEPRECATION_BANNER_REMOVAL_LINK)};
    String message = locale.QueryString(IDS_XSLT_DEPRECATION_BANNER_TEXT);
    CreateAndAppendBanner(document, [&](ContainerNode* banner) {
      AppendLocalizedBannerText(document, banner, message, removal_link,
                                extension_link);
    });
  }
}
}  // namespace

Document* XSLTProcessor::CreateDocumentFromSource(
    const String& source_string,
    const String& source_encoding,
    const String& source_mime_type,
    Node* source_node,
    LocalFrame* frame) {
  if (!source_node->GetExecutionContext())
    return nullptr;

  KURL url = NullUrl();
  Document* owner_document = &source_node->GetDocument();
  if (owner_document == source_node)
    url = owner_document->Url();
  String document_source = source_string;

  if (frame && owner_document->IsCAPAlert()) {
    UseCounter::Count(owner_document, WebFeature::kXmlCAPAlertWithXSLT);
  }

  bool is_cap_alert_xslt =
      frame && owner_document->IsCAPAlert() &&
      RuntimeEnabledFeatures::EnableXSLTForCAPAlertsEnabled(
          owner_document->GetExecutionContext());

  // Sites that have deployed an XSLT polyfill don't need the banner. This must
  // be computed before CommitNavigation() below, which detaches the source
  // document. CAP alerts are intentionally not scanned: they always get their
  // own banner.
  bool source_has_polyfill_script = !owner_document->IsCAPAlert() &&
                                    SourceHasPolyfillScript(*owner_document);

  String mime_type = source_mime_type;
  // Force text/plain to be parsed as XHTML. This was added without explanation
  // in 2005:
  // https://chromium.googlesource.com/chromium/src/+/e20d8de86f154892d94798bbd8b65720a11d6299
  // It's unclear whether it's still needed for compat.
  if (source_mime_type == "text/plain") {
    mime_type = "application/xhtml+xml";
    TransformTextStringToXHTMLDocumentString(document_source);
  }

  if (frame) {
    auto* previous_document_loader = frame->Loader().GetDocumentLoader();
    DCHECK(previous_document_loader);
    std::unique_ptr<WebNavigationParams> params =
        previous_document_loader->CreateWebNavigationParamsToCloneDocument();
    WebNavigationParams::FillStaticResponse(
        params.get(), mime_type,
        source_encoding.empty() ? "UTF-8" : source_encoding,
        StringUtf8Adaptor(document_source));
    params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
    frame->Loader().CommitNavigation(std::move(params), nullptr,
                                     CommitReason::kXSLT);
    Document* new_doc = frame->GetDocument();
    if (new_doc) {
      InjectXSLTWarningBanner(is_cap_alert_xslt, source_has_polyfill_script,
                              *new_doc);
    }
    return new_doc;
  }

  DocumentInit init =
      DocumentInit::Create()
          .WithURL(url)
          .WithTypeFrom(mime_type)
          .WithExecutionContext(owner_document->GetExecutionContext())
          .WithAgent(owner_document->GetAgent());
  Document* document = init.CreateDocument();
  auto parsed_source_encoding =
      source_encoding.empty() ? Utf8Encoding() : TextEncoding(source_encoding);
  if (parsed_source_encoding.IsValid()) {
    DocumentEncodingData data;
    data.SetEncoding(parsed_source_encoding);
    document->SetEncodingData(data);
  } else {
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kXml,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StrCat({"Document encoding not valid: ", source_encoding})));
  }
  document->SetContent(document_source);
  InjectXSLTWarningBanner(is_cap_alert_xslt, source_has_polyfill_script,
                          *document);
  return document;
}

Document* XSLTProcessor::transformToDocument(Node* source_node) {
  String result_mime_type;
  String result_string;
  String result_encoding;
  if (!TransformToString(source_node, result_mime_type, result_string,
                         result_encoding))
    return nullptr;
  return CreateDocumentFromSource(result_string, result_encoding,
                                  result_mime_type, source_node, nullptr);
}

DocumentFragment* XSLTProcessor::transformToFragment(Node* source_node,
                                                     Document* output_doc) {
  String result_mime_type;
  String result_string;
  String result_encoding;

  // If the output document is HTML, default to HTML method.
  if (IsA<HTMLDocument>(output_doc))
    result_mime_type = "text/html";

  if (!TransformToString(source_node, result_mime_type, result_string,
                         result_encoding))
    return nullptr;
  return CreateFragmentForTransformToFragment(result_string, result_mime_type,
                                              *output_doc);
}

void XSLTProcessor::setParameter(const String& /*namespaceURI*/,
                                 const String& local_name,
                                 const String& value) {
  // FIXME: namespace support?
  // should make a QualifiedName here but we'd have to expose the impl
  parameters_.Set(local_name, value);
}

String XSLTProcessor::getParameter(const String& /*namespaceURI*/,
                                   const String& local_name) const {
  // FIXME: namespace support?
  // should make a QualifiedName here but we'd have to expose the impl
  auto it = parameters_.find(local_name);
  if (it == parameters_.end())
    return String();
  return it->value;
}

void XSLTProcessor::removeParameter(const String& /*namespaceURI*/,
                                    const String& local_name) {
  // FIXME: namespace support?
  parameters_.erase(local_name);
}

void XSLTProcessor::reset() {
  stylesheet_.Clear();
  stylesheet_root_node_.Clear();
  parameters_.clear();
}

void XSLTProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(stylesheet_);
  visitor->Trace(stylesheet_root_node_);
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
