/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_H_

#include <bitset>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/timer/elapsed_timer.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink-forward.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_value_change.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/dom/document_encoding_data.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/live_node_list_registry.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/dom/text_link_colors.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/dom/user_action_element_set.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/loader/font_preload_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace network {
namespace mojom {
enum class CSPDisposition : int32_t;
}  // namespace mojom
}  // namespace network

namespace blink {

class AnimationClock;
class AXContext;
class AXObjectCache;
class Attr;
class BeforeUnloadEventListener;
class CDATASection;
class CSSStyleSheet;
class CanvasFontCache;
class ChromeClient;
class Comment;
class CompositorAnimationTimeline;
class ComputedAccessibleNode;
class DisplayLockDocumentState;
class ElementIntersectionObserverData;
class WindowAgent;
class ComputedStyle;
class ConsoleMessage;
class ContextFeatures;
class CookieJar;
class V0CustomElementMicrotaskRunQueue;
class V0CustomElementRegistrationContext;
class DOMImplementation;
class DOMWindow;
class DocumentFragment;
class DocumentInit;
class DocumentLoader;
class DocumentMarkerController;
class DocumentNameCollection;
class DocumentOutliveTimeReporter;
class DocumentParser;
class DocumentResourceCoordinator;
class DocumentState;
class DocumentAnimations;
class DocumentTimeline;
class DocumentType;
class DOMFeaturePolicy;
class Element;
class ElementDataCache;
class ElementRegistrationOptions;
class Event;
class EventFactoryBase;
class EventListener;
template <typename EventType>
class EventWithHitTestResults;
class FloatQuad;
class FloatRect;
class FontMatchingMetrics;
class FormController;
class HTMLAllCollection;
class HTMLBodyElement;
class FrameScheduler;
class HTMLCollection;
class HTMLDialogElement;
class HTMLElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLImportLoader;
class HTMLImportsController;
class HTMLLinkElement;
class HTMLScriptElementOrSVGScriptElement;
class HitTestRequest;
class HttpRefreshScheduler;
class IdleRequestOptions;
class IntersectionObserverController;
class LayoutView;
class LazyLoadImageObserver;
class LiveNodeListBase;
class LocalDOMWindow;
class Locale;
class LocalFrame;
class LocalFrameView;
class Location;
class MediaQueryListListener;
class MediaQueryMatcher;
class NavigationInitiatorImpl;
class NodeIterator;
class NthIndexCache;
class OriginAccessEntry;
class Page;
class PendingAnimations;
class ProcessingInstruction;
class PropertyRegistry;
class QualifiedName;
class Range;
class ResourceFetcher;
class RootScrollerController;
class ScriptValue;
class SVGDocumentExtensions;
class SVGUseElement;
class Text;
class TrustedHTML;
class ScriptElementBase;
class ScriptPromise;
class ScriptRunner;
class ScriptableDocumentParser;
class ScriptedAnimationController;
class SecurityContextInit;
class SecurityOrigin;
class SelectorQueryCache;
class SerializedScriptValue;
class Settings;
class SlotAssignmentEngine;
class SnapCoordinator;
class StringOrElementCreationOptions;
class StyleEngine;
class StyleResolver;
class StylePropertyMapReadOnly;
class StyleSheetList;
class TextAutosizer;
class TransformSource;
class TreeWalker;
class V8NodeFilter;
class ViewportData;
class VisitedLinkState;
class WebComputedAXTree;
class WebMouseEvent;
class WorkletAnimationController;
enum class CSSPropertyID;
struct AnnotatedRegionValue;
struct FocusParams;
struct IconURL;
struct PhysicalOffset;
struct WebPrintPageDescription;

using MouseEventWithHitTestResults = EventWithHitTestResults<WebMouseEvent>;

enum NodeListInvalidationType : int {
  kDoNotInvalidateOnAttributeChanges = 0,
  kInvalidateOnClassAttrChange,
  kInvalidateOnIdNameAttrChange,
  kInvalidateOnNameAttrChange,
  kInvalidateOnForAttrChange,
  kInvalidateForFormControls,
  kInvalidateOnHRefAttrChange,
  kInvalidateOnAnyAttrChange,
};
const int kNumNodeListInvalidationTypes = kInvalidateOnAnyAttrChange + 1;

enum DocumentClass {
  kDefaultDocumentClass = 0,
  kHTMLDocumentClass = 1,
  kXHTMLDocumentClass = 1 << 1,
  kImageDocumentClass = 1 << 2,
  kPluginDocumentClass = 1 << 3,
  kMediaDocumentClass = 1 << 4,
  kSVGDocumentClass = 1 << 5,
  kXMLDocumentClass = 1 << 6,
  kViewSourceDocumentClass = 1 << 7,
};

enum ShadowCascadeOrder {
  kShadowCascadeNone,
  kShadowCascadeV0,
  kShadowCascadeV1
};

using DocumentClassFlags = unsigned char;

// A map of IDL attribute name to Element value, for one particular element.
// For example,
//   el1.ariaActiveDescendant = el2
// would add the following pair to the ExplicitlySetAttrElementMap for el1:
//   ("ariaActiveDescendant", el2)
// This represents 'explicitly set attr-element' in the HTML specification.
// https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element-2
// Note that in the interest of simplicitly, attributes that reflect a single
// element reference are implemented using the same ExplicitlySetAttrElementsMap
// storage, but only store a single element vector which is DCHECKED at the
// calling site.
using ExplicitlySetAttrElementsMap =
    HeapHashMap<QualifiedName, Member<HeapVector<Member<Element>>>>;

// A document (https://dom.spec.whatwg.org/#concept-document) is the root node
// of a tree of DOM nodes, generally resulting from the parsing of an markup
// (typically, HTML) resource.
//
// A document may or may not have a browsing context
// (https://html.spec.whatwg.org/#browsing-context). A document with a browsing
// context is created by navigation, and has a non-null domWindow(), GetFrame(),
// Loader(), etc., and is visible to the user. It will have a valid
// GetExecutionContext(), which will be equal to domWindow(). If the Document
// constructor receives a DocumentInit created WithDocumentLoader(), it will
// have a browsing context.
// Documents created by all other APIs do not have a browsing context. These
// Documents still have a valid GetExecutionContext() (i.e., the domWindow() of
// the Document in which they were created), so they can still access
// script, but return null for domWindow(), GetFrame() and Loader(). Generally,
// they should not downcast the ExecutionContext to a LocalDOMWindow and access
// the properties of the window directly.
// Finally, unit tests are allowed to create a Document that does not even
// have a valid GetExecutionContext(). This is a lightweight way to test
// properties of the Document and the DOM that do not require script.
class CORE_EXPORT Document : public ContainerNode,
                             public TreeScope,
                             public UseCounter,
                             public FeaturePolicyParserDelegate,
                             public Supplementable<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Document);

 public:
  // Factory for web-exposed Document constructor. The argument document must be
  // a document instance representing window.document, and it works as the
  // source of ExecutionContext and security origin of the new document.
  // https://dom.spec.whatwg.org/#dom-document-document
  static Document* Create(Document&);

  explicit Document(const DocumentInit& init,
                    DocumentClassFlags flags = kDefaultDocumentClass);
  ~Document() override;

  // Constructs a Document instance without a subclass for testing.
  static Document* CreateForTest();

  static Range* CreateRangeAdjustedToTreeScope(const TreeScope&,
                                               const Position&);

  // Support JS introspection of frame policy (e.g. feature policy).
  DOMFeaturePolicy* featurePolicy();

  MediaQueryMatcher& GetMediaQueryMatcher();

  void MediaQueryAffectingValueChanged(MediaValueChange change);

  using TreeScope::getElementById;

  // TODO(crbug.com/1029822) Former ExecutionContext overrides. Most of these
  // should move to LocalDOMWindow.
  ContentSecurityPolicy* GetContentSecurityPolicyForWorld();
  LocalDOMWindow* ExecutingWindow() const;
  String OutgoingReferrer() const;
  network::mojom::ReferrerPolicy GetReferrerPolicy() const;

  // FeaturePolicyParserDelegate override
  // TODO(crbug.com/1029822) FeaturePolicyParserDelegate overrides, these
  // should migrate to LocalDOMWindow.
  bool FeatureEnabled(OriginTrialFeature) const override;
  void CountFeaturePolicyUsage(mojom::WebFeature feature) override;
  bool FeaturePolicyFeatureObserved(
      mojom::blink::FeaturePolicyFeature feature) override;

  bool DocumentPolicyFeatureObserved(
      mojom::blink::DocumentPolicyFeature feature);

  SecurityContext& GetSecurityContext() { return security_context_; }
  const SecurityContext& GetSecurityContext() const {
    return security_context_;
  }

  // TODO(crbug.com/1029822): Temporary helpers to access ExecutionContext
  // methods. These will need to be audited. Some might be useful permanent
  // helpers.
  const SecurityOrigin* GetSecurityOrigin() const;
  SecurityOrigin* GetMutableSecurityOrigin();
  ContentSecurityPolicy* GetContentSecurityPolicy() const;
  network::mojom::blink::WebSandboxFlags GetSandboxFlags() const;
  bool IsSandboxed(network::mojom::blink::WebSandboxFlags mask) const;
  SecureContextMode GetSecureContextMode() const;
  bool IsSecureContext() const;
  bool IsSecureContext(String& error_message) const;
  void SetSecureContextModeForTesting(SecureContextMode);
  void SetReferrerPolicy(network::mojom::ReferrerPolicy);
  Agent* GetAgent() const;
  OriginTrialContext* GetOriginTrialContext() const;
  bool IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature,
      ReportOptions report_on_failure = ReportOptions::kDoNotReport,
      const String& message = g_empty_string) const;
  bool IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature,
      ReportOptions report_option = ReportOptions::kDoNotReport,
      const String& message = g_empty_string,
      const String& source_file = g_empty_string) const;
  bool IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature,
      PolicyValue threshold_value,
      ReportOptions report_option = ReportOptions::kDoNotReport,
      const String& message = g_empty_string,
      const String& source_file = g_empty_string) const;

  String addressSpaceForBindings(ScriptState*) const;

  bool CanContainRangeEndPoint() const override { return true; }

  SelectorQueryCache& GetSelectorQueryCache();

  // Focus Management.
  Element* ActiveElement() const;
  bool hasFocus() const;

  // DOM methods & attributes for Document

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy, kBeforecopy)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut, kBeforecut)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste, kBeforepaste)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(freeze, kFreeze)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockchange, kPointerlockchange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockerror, kPointerlockerror)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange, kReadystatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resume, kResume)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search, kSearch)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation,
                                  kSecuritypolicyviolation)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(visibilitychange, kVisibilitychange)

  ViewportData& GetViewportData() const { return *viewport_data_; }

  void SetDoctype(DocumentType*);
  DocumentType* doctype() const { return doc_type_.Get(); }

  DOMImplementation& implementation();

  Element* documentElement() const { return document_element_.Get(); }

  Location* location() const;

  Element* CreateElementForBinding(const AtomicString& local_name,
                                   ExceptionState& = ASSERT_NO_EXCEPTION);
  Element* CreateElementForBinding(const AtomicString& local_name,
                                   const StringOrElementCreationOptions&,
                                   ExceptionState&);
  Element* createElementNS(const AtomicString& namespace_uri,
                           const AtomicString& qualified_name,
                           ExceptionState&);
  Element* createElementNS(const AtomicString& namespace_uri,
                           const AtomicString& qualified_name,
                           const StringOrElementCreationOptions&,
                           ExceptionState&);
  DocumentFragment* createDocumentFragment();
  Text* createTextNode(const String& data);
  Comment* createComment(const String& data);
  CDATASection* createCDATASection(const String& data, ExceptionState&);
  ProcessingInstruction* createProcessingInstruction(const String& target,
                                                     const String& data,
                                                     ExceptionState&);
  Attr* createAttribute(const AtomicString& name, ExceptionState&);
  Attr* createAttributeNS(const AtomicString& namespace_uri,
                          const AtomicString& qualified_name,
                          ExceptionState&);
  Node* importNode(Node* imported_node, bool deep, ExceptionState&);

  // "create an element" defined in DOM standard. This supports both of
  // autonomous custom elements and customized built-in elements.
  Element* CreateElement(const QualifiedName&,
                         const CreateElementFlags,
                         const AtomicString& is);
  // Creates an element without custom element processing.
  Element* CreateRawElement(const QualifiedName&,
                            const CreateElementFlags = CreateElementFlags());

  Element* ElementFromPoint(double x, double y) const;
  HeapVector<Member<Element>> ElementsFromPoint(double x, double y) const;
  Range* caretRangeFromPoint(int x, int y);
  Element* scrollingElement();
  // When calling from C++ code, use this method. scrollingElement() is
  // just for the web IDL implementation.
  Element* ScrollingElementNoLayout();

  String readyState() const;

  AtomicString characterSet() const { return Document::EncodingName(); }

  AtomicString EncodingName() const;

  void SetContent(const String&);

  String SuggestedMIMEType() const;
  void SetMimeType(const AtomicString&);
  AtomicString contentType() const;  // DOM 4 document.contentType

  const AtomicString& ContentLanguage() const { return content_language_; }
  void SetContentLanguage(const AtomicString&);

  String xmlEncoding() const { return xml_encoding_; }
  String xmlVersion() const { return xml_version_; }
  enum StandaloneStatus { kStandaloneUnspecified, kStandalone, kNotStandalone };
  bool xmlStandalone() const { return xml_standalone_ == kStandalone; }
  StandaloneStatus XmlStandaloneStatus() const {
    return static_cast<StandaloneStatus>(xml_standalone_);
  }
  bool HasXMLDeclaration() const { return has_xml_declaration_; }

  void SetXMLEncoding(const String& encoding) {
    xml_encoding_ = encoding;
  }  // read-only property, only to be set from XMLDocumentParser
  void setXMLVersion(const String&, ExceptionState&);
  void setXMLStandalone(bool, ExceptionState&);
  void SetHasXMLDeclaration(bool has_xml_declaration) {
    has_xml_declaration_ = has_xml_declaration ? 1 : 0;
  }

  String visibilityState() const;
  bool IsPageVisible() const;
  bool hidden() const;
  void DidChangeVisibilityState();

  bool wasDiscarded() const;
  void SetWasDiscarded(bool);

  // If the document is "prefetch only", it will not be fully contstructed,
  // and should never be displayed. Only a few resources will be loaded and
  // scanned, in order to warm up caches.
  bool IsPrefetchOnly() const;

  Node* adoptNode(Node* source, ExceptionState&);

  HTMLCollection* images();
  HTMLCollection* embeds();
  HTMLCollection* applets();
  HTMLCollection* links();
  HTMLCollection* forms();
  HTMLCollection* anchors();
  HTMLCollection* scripts();
  HTMLAllCollection* all();

  HTMLCollection* WindowNamedItems(const AtomicString& name);
  DocumentNameCollection* DocumentNamedItems(const AtomicString& name);
  HTMLCollection* DocumentAllNamedItems(const AtomicString& name);

  // "defaultView" attribute defined in HTML spec.
  DOMWindow* defaultView() const;

  bool IsHTMLDocument() const { return document_classes_ & kHTMLDocumentClass; }
  bool IsXHTMLDocument() const {
    return document_classes_ & kXHTMLDocumentClass;
  }
  bool IsXMLDocument() const { return document_classes_ & kXMLDocumentClass; }
  bool IsImageDocument() const {
    return document_classes_ & kImageDocumentClass;
  }
  bool IsSVGDocument() const { return document_classes_ & kSVGDocumentClass; }
  bool IsPluginDocument() const {
    return document_classes_ & kPluginDocumentClass;
  }
  bool IsMediaDocument() const {
    return document_classes_ & kMediaDocumentClass;
  }

  bool HasSVGRootNode() const;

  bool IsFrameSet() const;

  bool IsSrcdocDocument() const { return is_srcdoc_document_; }
  bool IsMobileDocument() const { return is_mobile_document_; }

  StyleResolver* GetStyleResolver() const;
  StyleResolver& EnsureStyleResolver() const;

  bool IsViewSource() const { return is_view_source_; }
  void SetIsViewSource(bool);

  // WebXR DOM Overlay support, cf https://immersive-web.github.io/dom-overlays/
  // True if there's an ongoing "immersive-ar" WebXR session with a DOM Overlay
  // element active. This is needed for applying the :xr-overlay pseudoclass
  // and compositing/paint integration for this mode.
  bool IsXrOverlay() const { return is_xr_overlay_; }
  // Called from modules/xr's XRSystem when DOM Overlay mode starts and ends.
  // This lazy-loads the UA stylesheet and updates the overlay element's
  // pseudoclass.
  void SetIsXrOverlay(bool enabled, Element* overlay_element);

  bool SawElementsInKnownNamespaces() const {
    return saw_elements_in_known_namespaces_;
  }

  bool IsScriptExecutionReady() const {
    return HaveImportsLoaded() && HaveScriptBlockingStylesheetsLoaded();
  }

  bool IsForExternalHandler() const { return is_for_external_handler_; }

  // This is a DOM function.
  StyleSheetList& StyleSheets();

  StyleEngine& GetStyleEngine() {
    DCHECK(style_engine_.Get());
    return *style_engine_.Get();
  }

  void ScheduleUseShadowTreeUpdate(SVGUseElement&);
  void UnscheduleUseShadowTreeUpdate(SVGUseElement&);

  void EvaluateMediaQueryList();

  FormController& GetFormController();
  DocumentState* GetDocumentState() const;
  void SetStateForNewControls(const Vector<String>&);

  LocalFrameView* View() const;  // can be null
  LocalFrame* GetFrame() const;  // can be null
  // Returns frame_ for current document, or if this is an HTML import, master
  // document's frame_, if any.  Can be null.
  // TODO(kochi): Audit usage of this interface (crbug.com/746150).
  LocalFrame* GetFrameOfMasterDocument() const;
  Page* GetPage() const;          // can be null
  Settings* GetSettings() const;  // can be null

  float DevicePixelRatio() const;

  Range* createRange();

  NodeIterator* createNodeIterator(Node* root,
                                   unsigned what_to_show,
                                   V8NodeFilter*);
  TreeWalker* createTreeWalker(Node* root,
                               unsigned what_to_show,
                               V8NodeFilter*);

  // Special support for editing
  Text* CreateEditingTextNode(const String&);

  void SetupFontBuilder(ComputedStyle& document_style);

  bool NeedsLayoutTreeUpdate() const;
  // Whether we need layout tree update for this node or not, without
  // considering nodes in display locked subtrees.
  bool NeedsLayoutTreeUpdateForNode(const Node&,
                                    bool ignore_adjacent_style = false) const;
  // Whether we need layout tree update for this node or not, including nodes in
  // display locked subtrees.
  bool NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
      const Node&,
      bool ignore_adjacent_style = false) const;

  // Update ComputedStyles and attach LayoutObjects if necessary, but don't
  // lay out.
  void UpdateStyleAndLayoutTree();
  void UpdateStyleAndLayoutTreeForNode(const Node*);
  void UpdateStyleAndLayoutTreeForSubtree(const Node*);

  void UpdateStyleAndLayout(DocumentUpdateReason);
  void LayoutUpdated();
  enum RunPostLayoutTasks {
    kRunPostLayoutTasksAsynchronously,
    kRunPostLayoutTasksSynchronously,
  };
  void UpdateStyleAndLayoutForNode(const Node*, DocumentUpdateReason);
  void IncLayoutCallsCounter() { ++layout_calls_counter_; }
  void IncLayoutCallsCounterNG() { ++layout_calls_counter_ng_; }
  void IncLayoutBlockCounter() { ++layout_blocks_counter_; }
  void IncLayoutBlockCounterNG() { ++layout_blocks_counter_ng_; }

  scoped_refptr<const ComputedStyle> StyleForPage(int page_index);

  // Ensures that location-based data will be valid for a given node.
  //
  // This will run style and layout if they are currently dirty, and it may also
  // run compositing inputs if the node is in a sticky subtree (as the sticky
  // offset may change the node's position).
  //
  // Due to this you should only call this if you definitely need valid location
  // data, otherwise use one of the |UpdateStyleAndLayout...| methods above.
  void EnsurePaintLocationDataValidForNode(const Node*,
                                           DocumentUpdateReason reason);

  // Returns true if page box (margin boxes and page borders) is visible.
  bool IsPageBoxVisible(int page_index);

  // Gets the description for the specified page. This includes preferred page
  // size and margins in pixels, assuming 96 pixels per inch. The size and
  // margins must be initialized to the default values that are used if auto is
  // specified.
  void GetPageDescription(int page_index, WebPrintPageDescription*);

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

  void Initialize();
  virtual void Shutdown();

  void InitContentSecurityPolicy(ContentSecurityPolicy*);

  // If you have a Document, use GetLayoutView() instead which is faster.
  void GetLayoutObject() const = delete;

  LayoutView* GetLayoutView() const { return layout_view_; }

  // This will return an AXObjectCache only if there's one or more
  // AXContext associated with this document. When all associated
  // AXContexts are deleted, the AXObjectCache will be removed.
  AXObjectCache* ExistingAXObjectCache() const;

  Document& AXObjectCacheOwner() const;
  void ClearAXObjectCache();

  // to get visually ordered hebrew and arabic pages right
  bool VisuallyOrdered() const { return visually_ordered_; }

  DocumentLoader* Loader() const;

  // This is the DOM API document.open(). enteredDocument is the responsible
  // document of the entry settings object.
  void open(Document* entered_document, ExceptionState&);
  // This is used internally and does not handle exceptions.
  void open();
  DocumentParser* OpenForNavigation(ParserSynchronizationPolicy,
                                    const AtomicString& mime_type,
                                    const AtomicString& encoding);
  DocumentParser* ImplicitOpen(ParserSynchronizationPolicy);

  // This is the DOM API document.open() implementation.
  // document.open() opens a new window when called with three arguments.
  Document* open(v8::Isolate*,
                 const AtomicString& type,
                 const AtomicString& replace,
                 ExceptionState&);
  DOMWindow* open(v8::Isolate*,
                  const String& url_string,
                  const AtomicString& name,
                  const AtomicString& features,
                  ExceptionState&);
  // This is the DOM API document.close().
  void close(ExceptionState&);
  // This is used internally and does not handle exceptions.
  void close();

  // Corresponds to "9. Abort the active document of browsingContext."
  // https://html.spec.whatwg.org/C/#navigate
  void Abort();

  void CheckCompleted();

  // Dispatches beforeunload into this document. Returns true if the
  // beforeunload handler indicates that it is safe to proceed with an unload,
  // false otherwise.
  //
  // |chrome_client| is used to synchronously get user consent (via a modal
  // javascript dialog) to allow the unload to proceed if the beforeunload
  // handler returns a non-null value, indicating unsaved state. If a
  // null |chrome_client| is provided and the beforeunload returns a non-null
  // value this function will automatically return false, indicating that the
  // unload should not proceed. A null chrome client is set to by the freezing
  // logic, which uses this to determine if a non-empty beforeunload handler
  // is present before allowing discarding to proceed.
  //
  // |is_reload| indicates if the beforeunload is being triggered because of a
  // reload operation, otherwise it is assumed to be a page close or navigation.
  //
  // |did_allow_navigation| is set to reflect the choice made by the user via
  // the modal dialog. The value is meaningless if |auto_cancel|
  // is true, in which case it will always be set to false.
  bool DispatchBeforeUnloadEvent(ChromeClient* chrome_client,
                                 bool is_reload,
                                 bool& did_allow_navigation);

  struct UnloadEventTiming {
    base::TimeTicks unload_event_start;
    base::TimeTicks unload_event_end;
  };
  // Dispatches "pagehide", "visibilitychange" and "unload" events, if not
  // dispatched already. Fills unload timing if present and |committing_origin|
  // has access to the unload timing of the document.
  void DispatchUnloadEvents(SecurityOrigin* committing_origin,
                            base::Optional<Document::UnloadEventTiming>*);

  void DispatchFreezeEvent();

  enum PageDismissalType {
    kNoDismissal,
    kBeforeUnloadDismissal,
    kPageHideDismissal,
    kUnloadVisibilityChangeDismissal,
    kUnloadDismissal
  };
  PageDismissalType PageDismissalEventBeingDispatched() const;

  void CancelParsing();

  void write(const String& text,
             Document* entered_document = nullptr,
             ExceptionState& = ASSERT_NO_EXCEPTION);
  void writeln(const String& text,
               Document* entered_document = nullptr,
               ExceptionState& = ASSERT_NO_EXCEPTION);
  void write(v8::Isolate*, const Vector<String>& text, ExceptionState&);
  void writeln(v8::Isolate*, const Vector<String>& text, ExceptionState&);

  // TrustedHTML variants of the above.
  // TODO(mkwst): Write a spec for this.
  void write(v8::Isolate*, TrustedHTML*, ExceptionState&);
  void writeln(v8::Isolate*, TrustedHTML*, ExceptionState&);

  bool WellFormed() const { return well_formed_; }

  // Return the document URL, or an empty URL if it's unavailable.
  // This is not an implementation of web-exposed Document.prototype.URL.
  const KURL& Url() const { return url_; }
  void SetURL(const KURL&);

  // Bind the url to document.url, if unavailable bind to about:blank.
  KURL urlForBinding() const;

  // To understand how these concepts relate to one another, please see the
  // comments surrounding their declaration.

  // Document base URL.
  // https://html.spec.whatwg.org/C/#document-base-url
  const KURL& BaseURL() const;
  void SetBaseURLOverride(const KURL&);
  const KURL& BaseURLOverride() const { return base_url_override_; }
  KURL ValidBaseElementURL() const;
  const AtomicString& BaseTarget() const { return base_target_; }
  void ProcessBaseElement();

  // Fallback base URL.
  // https://html.spec.whatwg.org/C/#fallback-base-url
  KURL FallbackBaseURL() const;

  // Creates URL based on passed relative url and this documents base URL.
  // Depending on base URL value it is possible that parent document
  // base URL will be used instead. Uses CompleteURLWithOverride internally.
  KURL CompleteURL(const String&) const;
  // Creates URL based on passed relative url and passed base URL override.
  KURL CompleteURLWithOverride(const String&,
                               const KURL& base_url_override) const;

  const KURL& WebBundleClaimedUrl() const { return web_bundle_claimed_url_; }

  // Determines whether a new document should take on the same origin as that of
  // the document which created it.
  static bool ShouldInheritSecurityOriginFromOwner(const KURL&);

  CSSStyleSheet& ElementSheet();

  virtual DocumentParser* CreateParser();
  DocumentParser* Parser() const { return parser_.Get(); }
  ScriptableDocumentParser* GetScriptableDocumentParser() const;

  // FinishingPrinting denotes that the non-printing layout state is being
  // restored.
  enum PrintingState {
    kNotPrinting,
    kBeforePrinting,
    kPrinting,
    kFinishingPrinting
  };
  bool Printing() const { return printing_ == kPrinting; }
  bool BeforePrintingOrPrinting() const {
    return printing_ == kPrinting || printing_ == kBeforePrinting;
  }
  bool FinishingOrIsPrinting() {
    return printing_ == kPrinting || printing_ == kFinishingPrinting;
  }
  void SetPrinting(PrintingState);

  bool IsPaintingPreview() const { return is_painting_preview_; }
  bool IsCapturingLayout() const {
    return printing_ == kPrinting || is_painting_preview_;
  }
  void SetIsPaintingPreview(bool);

  enum CompatibilityMode { kQuirksMode, kLimitedQuirksMode, kNoQuirksMode };

  void SetCompatibilityMode(CompatibilityMode);
  CompatibilityMode GetCompatibilityMode() const { return compatibility_mode_; }

  String compatMode() const;

  bool InQuirksMode() const { return compatibility_mode_ == kQuirksMode; }
  bool InLimitedQuirksMode() const {
    return compatibility_mode_ == kLimitedQuirksMode;
  }
  bool InNoQuirksMode() const { return compatibility_mode_ == kNoQuirksMode; }
  bool InLineHeightQuirksMode() const { return !InNoQuirksMode(); }

  // https://html.spec.whatwg.org/C/#documentreadystate
  enum DocumentReadyState { kLoading, kInteractive, kComplete };

  DocumentReadyState GetReadyState() const { return ready_state_; }
  void SetReadyState(DocumentReadyState);
  bool IsLoadCompleted() const;

  bool IsFreezingInProgress() const { return is_freezing_in_progress_; }

  enum ParsingState { kParsing, kInDOMContentLoaded, kFinishedParsing };
  void SetParsingState(ParsingState);
  bool Parsing() const { return parsing_state_ == kParsing; }
  bool HasFinishedParsing() const { return parsing_state_ == kFinishedParsing; }

  bool ShouldScheduleLayout() const;

  TextLinkColors& GetTextLinkColors() { return text_link_colors_; }
  const TextLinkColors& GetTextLinkColors() const { return text_link_colors_; }
  VisitedLinkState& GetVisitedLinkState() const { return *visited_link_state_; }

  MouseEventWithHitTestResults PerformMouseEventHitTest(const HitTestRequest&,
                                                        const PhysicalOffset&,
                                                        const WebMouseEvent&);

  void SetHadKeyboardEvent(bool had_keyboard_event) {
    had_keyboard_event_ = had_keyboard_event;
  }
  bool HadKeyboardEvent() const { return had_keyboard_event_; }
  void SetLastFocusType(mojom::blink::FocusType last_focus_type);
  mojom::blink::FocusType LastFocusType() const { return last_focus_type_; }
  bool SetFocusedElement(Element*, const FocusParams&);
  void ClearFocusedElement();
  Element* FocusedElement() const { return focused_element_.Get(); }
  UserActionElementSet& UserActionElements() { return user_action_elements_; }
  const UserActionElementSet& UserActionElements() const {
    return user_action_elements_;
  }

  ExplicitlySetAttrElementsMap* GetExplicitlySetAttrElementsMap(Element*);

  // Returns false if the function fails.  e.g. |pseudo| is not supported.
  bool SetPseudoStateForTesting(Element& element,
                                const String& pseudo,
                                bool matches);
  void EnqueueAutofocusCandidate(Element&);
  bool HasAutofocusCandidates() const;
  void FlushAutofocusCandidates();
  void FinalizeAutofocus();
  void SetSequentialFocusNavigationStartingPoint(Node*);
  Element* SequentialFocusNavigationStartingPoint(
      mojom::blink::FocusType) const;

  void SetActiveElement(Element*);
  Element* GetActiveElement() const { return active_element_.Get(); }

  Element* HoverElement() const { return hover_element_.Get(); }

  void RemoveFocusedElementOfSubtree(Node&, bool among_children_only = false);
  void HoveredElementDetached(Element&);
  void ActiveChainNodeDetached(Element&);

  // Updates hover and active state of elements in the Document. The
  // |is_active| param specifies whether the active state should be set or
  // unset. |update_active_chain| is used to prevent updates to elements
  // outside the frozen active chain; passing false will only refresh the
  // active state of elements in the existing chain, but not outside of it. The
  // given element is the inner-most element whose state is being modified.
  // Hover is always applied.
  void UpdateHoverActiveState(bool is_active,
                              bool update_active_chain,
                              Element*);

  // Updates for :target (CSS3 selector).
  void SetCSSTarget(Element*);
  Element* CssTarget() const { return css_target_; }

  void ScheduleLayoutTreeUpdateIfNeeded();
  bool HasPendingForcedStyleRecalc() const;

  void RegisterNodeList(const LiveNodeListBase*);
  void UnregisterNodeList(const LiveNodeListBase*);
  void RegisterNodeListWithIdNameCache(const LiveNodeListBase*);
  void UnregisterNodeListWithIdNameCache(const LiveNodeListBase*);
  bool ShouldInvalidateNodeListCaches(
      const QualifiedName* attr_name = nullptr) const;
  void InvalidateNodeListCaches(const QualifiedName* attr_name);

  void AttachNodeIterator(NodeIterator*);
  void DetachNodeIterator(NodeIterator*);
  void MoveNodeIteratorsToNewDocument(Node&, Document&);

  void AttachRange(Range*);
  void DetachRange(Range*);

  void DidMoveTreeToNewDocument(const Node& root);
  // nodeChildrenWillBeRemoved is used when removing all node children at once.
  void NodeChildrenWillBeRemoved(ContainerNode&);
  // nodeWillBeRemoved is only safe when removing one node at a time.
  void NodeWillBeRemoved(Node&);
  bool CanAcceptChild(const Node& new_child,
                      const Node* next,
                      const Node* old_child,
                      ExceptionState&) const;

  void DidInsertText(const CharacterData&, unsigned offset, unsigned length);
  void DidRemoveText(const CharacterData&, unsigned offset, unsigned length);
  void DidMergeTextNodes(const Text& merged_node,
                         const Text& node_to_be_removed,
                         unsigned old_length);
  void DidSplitTextNode(const Text& old_node);

  LocalDOMWindow* domWindow() const { return dom_window_; }

  // Helper functions for forwarding LocalDOMWindow event related tasks to the
  // LocalDOMWindow if it exists.
  void SetWindowAttributeEventListener(const AtomicString& event_type,
                                       EventListener*);
  EventListener* GetWindowAttributeEventListener(
      const AtomicString& event_type);

  static void RegisterEventFactory(std::unique_ptr<EventFactoryBase>);
  static Event* createEvent(ScriptState*,
                            const String& event_type,
                            ExceptionState&);

  // keep track of what types of event listeners are registered, so we don't
  // dispatch events unnecessarily
  enum ListenerType {
    kDOMSubtreeModifiedListener = 1,
    kDOMNodeInsertedListener = 1 << 1,
    kDOMNodeRemovedListener = 1 << 2,
    kDOMNodeRemovedFromDocumentListener = 1 << 3,
    kDOMNodeInsertedIntoDocumentListener = 1 << 4,
    kDOMCharacterDataModifiedListener = 1 << 5,
    kAnimationEndListener = 1 << 6,
    kAnimationStartListener = 1 << 7,
    kAnimationIterationListener = 1 << 8,
    kAnimationCancelListener = 1 << 9,
    kTransitionRunListener = 1 << 10,
    kTransitionStartListener = 1 << 11,
    kTransitionEndListener = 1 << 12,
    kTransitionCancelListener = 1 << 13,
    kScrollListener = 1 << 14,
    kLoadListenerAtCapturePhaseOrAtStyleElement = 1 << 15,
    // 0 bits remaining
  };

  bool HasListenerType(ListenerType listener_type) const {
    return (listener_types_ & listener_type);
  }
  void AddListenerTypeIfNeeded(const AtomicString& event_type, EventTarget&);

  bool HasMutationObserversOfType(MutationType type) const {
    return mutation_observer_types_ & type;
  }
  bool HasMutationObservers() const { return mutation_observer_types_; }
  void AddMutationObserverTypes(MutationType types) {
    mutation_observer_types_ |= types;
  }

  IntersectionObserverController* GetIntersectionObserverController();
  IntersectionObserverController& EnsureIntersectionObserverController();

  // This is used to track IntersectionObservers for which this document is the
  // explicit root. The IntersectionObserverController tracks *all* observers
  // associated with this document; usually that's what you want.
  ElementIntersectionObserverData*
  DocumentExplicitRootIntersectionObserverData() const;
  ElementIntersectionObserverData&
  EnsureDocumentExplicitRootIntersectionObserverData();

  // Returns the owning element in the parent document. Returns nullptr if
  // this is the top level document or the owner is remote.
  HTMLFrameOwnerElement* LocalOwner() const;

  void WillChangeFrameOwnerProperties(int margin_width,
                                      int margin_height,
                                      mojom::blink::ScrollbarMode,
                                      bool is_display_none);

  String title() const { return title_; }
  void setTitle(const String&);

  Element* TitleElement() const { return title_element_.Get(); }
  void SetTitleElement(Element*);
  void RemoveTitle(Element* title_element);

  const AtomicString& dir();
  void setDir(const AtomicString&);

  String cookie(ExceptionState&) const;
  void setCookie(const String&, ExceptionState&);
  bool CookiesEnabled() const;

  const AtomicString& referrer() const;

  String domain() const;
  void setDomain(const String& new_domain, ExceptionState&);

  void OverrideLastModified(const AtomicString& modified) {
    override_last_modified_ = modified;
  }
  String lastModified() const;

  // The cookieURL is used to query the cookie database for this document's
  // cookies. For example, if the cookie URL is http://example.com, we'll
  // use the non-Secure cookies for example.com when computing
  // document.cookie.
  //
  // Q: How is the cookieURL different from the document's URL?
  // A: The two URLs are the same almost all the time.  However, if one
  //    document inherits the security context of another document, it
  //    inherits its cookieURL but not its URL.
  //
  const KURL& CookieURL() const { return cookie_url_; }
  void SetCookieURL(const KURL& url) { cookie_url_ = url; }

  // Returns null if the document is not attached to a frame.
  scoped_refptr<const SecurityOrigin> TopFrameOrigin() const;

  net::SiteForCookies SiteForCookies() const;

  // Permissions service helper methods to facilitate requesting and checking
  // storage access permissions.
  mojom::blink::PermissionService* GetPermissionService(
      ExecutionContext* execution_context);
  void PermissionServiceConnectionError();

  // Storage Access API methods to check for or request access to storage that
  // may otherwise be blocked.
  ScriptPromise hasStorageAccess(ScriptState* script_state);
  ScriptPromise requestStorageAccess(ScriptState* script_state);

  // Sends a query via Mojo to ask whether the user has any trust tokens. This
  // can reject on permissions errors (e.g. associating |issuer| with the
  // top-level origin would exceed the top-level origin's limit on the number of
  // associated issuers) or on other internal errors (e.g. the network service
  // is unavailable).
  ScriptPromise hasTrustToken(ScriptState* script_state,
                              const String& issuer,
                              ExceptionState&);

  // The following implements the rule from HTML 4 for what valid names are.
  // To get this right for all the XML cases, we probably have to improve this
  // or move it and make it sensitive to the type of document.
  static bool IsValidName(const String&);

  // The following breaks a qualified name into a prefix and a local name.
  // It also does a validity check, and returns false if the qualified name
  // is invalid.  It also sets ExceptionCode when name is invalid.
  static bool ParseQualifiedName(const AtomicString& qualified_name,
                                 AtomicString& prefix,
                                 AtomicString& local_name,
                                 ExceptionState&);

  // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
  static bool HasValidNamespaceForElements(const QualifiedName&);
  static bool HasValidNamespaceForAttributes(const QualifiedName&);

  // "body element" as defined by HTML5
  // (https://html.spec.whatwg.org/C/#the-body-element-2).
  // That is, the first body or frameset child of the document element.
  HTMLElement* body() const;

  // "HTML body element" as defined by CSSOM View spec
  // (https://drafts.csswg.org/cssom-view/#the-html-body-element).
  // That is, the first body child of the document element.
  HTMLBodyElement* FirstBodyElement() const;

  void setBody(HTMLElement*, ExceptionState&);
  void WillInsertBody();

  HTMLHeadElement* head() const;

  // Decide which element is to define the viewport's overflow policy.
  Element* ViewportDefiningElement() const;

  DocumentMarkerController& Markers() const { return *markers_; }

  // Support for Javascript execCommand, and related methods
  // See "core/editing/commands/document_exec_command.cc" for implementations.
  bool execCommand(const String& command,
                   bool show_ui,
                   const String& value,
                   ExceptionState&);
  bool IsRunningExecCommand() const { return is_running_exec_command_; }
  bool queryCommandEnabled(const String& command, ExceptionState&);
  bool queryCommandIndeterm(const String& command, ExceptionState&);
  bool queryCommandState(const String& command, ExceptionState&);
  bool queryCommandSupported(const String& command, ExceptionState&);
  String queryCommandValue(const String& command, ExceptionState&);

  KURL OpenSearchDescriptionURL();

  // designMode support
  bool InDesignMode() const { return design_mode_; }
  String designMode() const;
  void setDesignMode(const String&);

  // The document of the parent frame.
  Document* ParentDocument() const;
  Document& TopDocument() const;

  // Will only return nullptr in unit tests.
  ExecutionContext* GetExecutionContext() const final;

  ScriptRunner* GetScriptRunner() { return script_runner_.Get(); }

  void currentScriptForBinding(HTMLScriptElementOrSVGScriptElement&) const;
  void PushCurrentScript(ScriptElementBase*);
  void PopCurrentScript(ScriptElementBase*);

  void SetTransformSource(std::unique_ptr<TransformSource>);
  TransformSource* GetTransformSource() const {
    return transform_source_.get();
  }

  void IncDOMTreeVersion() {
    DCHECK(lifecycle_.StateAllowsTreeMutations());
    dom_tree_version_ = ++global_tree_version_;
  }
  uint64_t DomTreeVersion() const { return dom_tree_version_; }

  uint64_t StyleVersion() const { return style_version_; }

  enum PendingSheetLayout {
    kNoLayoutWithPendingSheets,
    kDidLayoutWithPendingSheets,
    kIgnoreLayoutWithPendingSheets
  };

  Vector<IconURL> IconURLs(int icon_types_mask);

  base::Optional<Color> ThemeColor() const;

  // Returns the HTMLLinkElement currently in use for the Web Manifest.
  // Returns null if there is no such element.
  HTMLLinkElement* LinkManifest() const;

  // Returns the HTMLLinkElement holding the canonical URL. Returns null if
  // there is no such element.
  HTMLLinkElement* LinkCanonical() const;

  void UpdateFocusAppearanceAfterLayout();
  void CancelFocusAppearanceUpdate();
  // Return true after UpdateFocusAppearanceAfterLayout() call and before
  // updating focus appearance.
  bool WillUpdateFocusAppearance() const;

  bool IsDNSPrefetchEnabled() const { return is_dns_prefetch_enabled_; }
  void ParseDNSPrefetchControlHeader(const String&);

  void FinishedParsing();

  void SetEncodingData(const DocumentEncodingData& new_data);
  const WTF::TextEncoding& Encoding() const {
    return encoding_data_.Encoding();
  }

  bool EncodingWasDetectedHeuristically() const {
    return encoding_data_.WasDetectedHeuristically();
  }
  bool SawDecodingError() const { return encoding_data_.SawDecodingError(); }

  void SetAnnotatedRegionsDirty(bool f) { annotated_regions_dirty_ = f; }
  bool AnnotatedRegionsDirty() const { return annotated_regions_dirty_; }
  bool HasAnnotatedRegions() const { return has_annotated_regions_; }
  void SetHasAnnotatedRegions(bool f) { has_annotated_regions_ = f; }
  const Vector<AnnotatedRegionValue>& AnnotatedRegions() const;
  void SetAnnotatedRegions(const Vector<AnnotatedRegionValue>&);

  void RemoveAllEventListeners() final;

  const SVGDocumentExtensions* SvgExtensions();
  SVGDocumentExtensions& AccessSVGExtensions();

  bool AllowInlineEventHandler(Node*,
                               EventListener*,
                               const String& context_url,
                               const WTF::OrdinalNumber& context_line);

  void StatePopped(scoped_refptr<SerializedScriptValue>);

  enum LoadEventProgress {
    kLoadEventNotRun,
    kLoadEventInProgress,
    kLoadEventCompleted,
    kBeforeUnloadEventInProgress,
    kBeforeUnloadEventCompleted,
    kPageHideInProgress,
    kUnloadVisibilityChangeInProgress,
    kUnloadEventInProgress,
    kUnloadEventHandled
  };
  bool LoadEventStillNeeded() const {
    return load_event_progress_ == kLoadEventNotRun;
  }
  bool LoadEventFinished() const {
    return load_event_progress_ >= kLoadEventCompleted;
  }
  bool UnloadStarted() const {
    return load_event_progress_ >= kPageHideInProgress;
  }
  bool ProcessingBeforeUnload() const {
    return load_event_progress_ == kBeforeUnloadEventInProgress;
  }

  void SetContainsPlugins() { contains_plugins_ = true; }
  bool ContainsPlugins() const { return contains_plugins_; }

  void EnqueueResizeEvent();
  void EnqueueScrollEventForNode(Node*);
  void EnqueueScrollEndEventForNode(Node*);
  void EnqueueOverscrollEventForNode(Node* target,
                                     double delta_x,
                                     double delta_y);
  void EnqueueDisplayLockActivationTask(base::OnceClosure);
  void EnqueueAnimationFrameTask(base::OnceClosure);
  void EnqueueAnimationFrameEvent(Event*);
  // Only one event for a target/event type combination will be dispatched per
  // frame.
  void EnqueueUniqueAnimationFrameEvent(Event*);
  void EnqueueMediaQueryChangeListeners(
      HeapVector<Member<MediaQueryListListener>>&);
  void EnqueueVisualViewportScrollEvent();
  void EnqueueVisualViewportResizeEvent();

  void DispatchEventsForPrinting();

  void exitPointerLock();
  Element* PointerLockElement() const;

  // Used to allow element that loads data without going through a FrameLoader
  // to delay the 'load' event.
  void IncrementLoadEventDelayCount() { ++load_event_delay_count_; }
  void DecrementLoadEventDelayCount();
  void CheckLoadEventSoon();
  bool IsDelayingLoadEvent();
  void LoadPluginsSoon();
  // This calls checkCompleted() sync and thus can cause JavaScript execution.
  void DecrementLoadEventDelayCountAndCheckLoadEvent();

  const DocumentTiming& GetTiming() const { return document_timing_; }

  int RequestAnimationFrame(FrameRequestCallbackCollection::FrameCallback*);
  void CancelAnimationFrame(int id);
  void ServiceScriptedAnimations(
      base::TimeTicks monotonic_animation_start_time);
  void SetCurrentFrameIsThrottled(bool throttled) {
    current_frame_is_throttled_ = true;
  }

  int RequestPostAnimationFrame(FrameRequestCallbackCollection::FrameCallback*);
  void CancelPostAnimationFrame(int id);
  void RunPostAnimationFrameCallbacks();

  int RequestIdleCallback(ScriptedIdleTaskController::IdleTask*,
                          const IdleRequestOptions*);
  void CancelIdleCallback(int id);

  ScriptedAnimationController& GetScriptedAnimationController();

  void InitDNSPrefetch();

  bool IsInDocumentWrite() const { return write_recursion_depth_ > 0; }

  TextAutosizer* GetTextAutosizer();

  ScriptValue registerElement(ScriptState*,
                              const AtomicString& name,
                              const ElementRegistrationOptions*,
                              ExceptionState&);
  V0CustomElementRegistrationContext* RegistrationContext() const;
  V0CustomElementMicrotaskRunQueue* CustomElementMicrotaskRunQueue();

  void ClearImportsController();
  HTMLImportsController* EnsureImportsController();
  HTMLImportsController* ImportsController() const {
    return imports_controller_;
  }
  HTMLImportLoader* ImportLoader() const;

  bool IsHTMLImport() const;
  // TODO(kochi): Audit usage of this interface (crbug.com/746150).
  Document& MasterDocument() const;

  void DidLoadAllImports();

  void AdjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>&,
                                                const LayoutObject&) const;
  void AdjustFloatRectForScrollAndAbsoluteZoom(FloatRect&,
                                               const LayoutObject&) const;

  void SetContextFeatures(ContextFeatures&);
  ContextFeatures& GetContextFeatures() const { return *context_features_; }

  ElementDataCache* GetElementDataCache() { return element_data_cache_.Get(); }

  void DidLoadAllScriptBlockingResources();
  void DidAddPendingParserBlockingStylesheet();
  void DidLoadAllPendingParserBlockingStylesheets();
  void DidRemoveAllPendingStylesheets();

  bool InStyleRecalc() const {
    return lifecycle_.GetState() == DocumentLifecycle::kInStyleRecalc;
  }

  // Return a Locale for the default locale if the argument is null or empty.
  Locale& GetCachedLocale(const AtomicString& locale = g_null_atom);

  AnimationClock& GetAnimationClock();
  const AnimationClock& GetAnimationClock() const;
  DocumentAnimations& GetDocumentAnimations() const {
    return *document_animations_;
  }
  DocumentTimeline& Timeline() const { return *timeline_; }
  PendingAnimations& GetPendingAnimations() { return *pending_animations_; }
  WorkletAnimationController& GetWorkletAnimationController() {
    return *worklet_animation_controller_;
  }

  void AttachCompositorTimeline(CompositorAnimationTimeline*) const;
  void DetachCompositorTimeline(CompositorAnimationTimeline*) const;

  void AddToTopLayer(Element*, const Element* before = nullptr);
  void RemoveFromTopLayer(Element*);
  const HeapVector<Member<Element>>& TopLayerElements() const {
    return top_layer_elements_;
  }
  HTMLDialogElement* ActiveModalDialog() const;

  // A non-null template_document_host_ implies that |this| was created by
  // EnsureTemplateDocument().
  bool IsTemplateDocument() const { return !!template_document_host_; }
  Document& EnsureTemplateDocument();
  Document* TemplateDocumentHost() { return template_document_host_; }

  // TODO(thestig): Rename these and related functions, since we can call them
  // for controls outside of forms as well.
  void DidAssociateFormControl(Element*);

  void AddConsoleMessage(ConsoleMessage* message,
                         bool discard_duplicates = false) const;

  LocalFrame* ExecutingFrame();

  DocumentLifecycle& Lifecycle() { return lifecycle_; }
  const DocumentLifecycle& Lifecycle() const { return lifecycle_; }
  bool IsActive() const { return lifecycle_.IsActive(); }
  bool IsDetached() const {
    return lifecycle_.GetState() >= DocumentLifecycle::kStopping;
  }
  bool IsStopped() const {
    return lifecycle_.GetState() == DocumentLifecycle::kStopped;
  }

  enum HttpRefreshType { kHttpRefreshFromHeader, kHttpRefreshFromMetaTag };
  void MaybeHandleHttpRefresh(const String&, HttpRefreshType);
  bool IsHttpRefreshScheduledWithin(base::TimeDelta interval);

  void SetHasViewportUnits() { has_viewport_units_ = true; }
  bool HasViewportUnits() const { return has_viewport_units_; }
  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();

  void UpdateActiveStyle();
  void InvalidateStyleAndLayoutForFontUpdates();

  void Trace(Visitor*) const override;

  AtomicString ConvertLocalName(const AtomicString&);

  void PlatformColorsChanged();

  NthIndexCache* GetNthIndexCache() const { return nth_index_cache_; }

  CanvasFontCache* GetCanvasFontCache();

  // Used by unit tests so that all parsing will be main thread for
  // controlling parsing and chunking precisely.
  static void SetThreadedParsingEnabledForTesting(bool);
  static bool ThreadedParsingEnabledForTesting();

  void IncrementNodeCount() { node_count_++; }
  void DecrementNodeCount() {
    DCHECK_GT(node_count_, 0);
    node_count_--;
  }
  int NodeCount() const { return node_count_; }

  SnapCoordinator& GetSnapCoordinator();
  void PerformScrollSnappingTasks();

  void DidEnforceInsecureRequestPolicy();
  void DidEnforceInsecureNavigationsSet();

  bool MayContainV0Shadow() const { return may_contain_v0_shadow_; }

  ShadowCascadeOrder GetShadowCascadeOrder() const {
    return shadow_cascade_order_;
  }
  void SetShadowCascadeOrder(ShadowCascadeOrder);

  bool ContainsV1ShadowTree() const {
    return shadow_cascade_order_ == ShadowCascadeOrder::kShadowCascadeV1;
  }

  Element* rootScroller() const;
  void setRootScroller(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  RootScrollerController& GetRootScrollerController() const {
    DCHECK(root_scroller_controller_);
    return *root_scroller_controller_;
  }

  bool IsInMainFrame() const;

  const PropertyRegistry* GetPropertyRegistry() const {
    return property_registry_;
  }
  PropertyRegistry& EnsurePropertyRegistry();

  // Used to notify the embedder when the user edits the value of a
  // text field in a non-secure context.
  void MaybeQueueSendDidEditFieldInInsecureContext();

  // May return nullptr when PerformanceManager instrumentation is disabled.
  DocumentResourceCoordinator* GetResourceCoordinator();

  // Apply pending feature policy headers and document policy headers.
  void ApplyPendingFramePolicyHeaders();

  const AtomicString& bgColor() const;
  void setBgColor(const AtomicString&);
  const AtomicString& fgColor() const;
  void setFgColor(const AtomicString&);
  const AtomicString& alinkColor() const;
  void setAlinkColor(const AtomicString&);
  const AtomicString& linkColor() const;
  void setLinkColor(const AtomicString&);
  const AtomicString& vlinkColor() const;
  void setVlinkColor(const AtomicString&);

  void clear() {}

  void captureEvents() {}
  void releaseEvents() {}

  ukm::UkmRecorder* UkmRecorder();
  ukm::SourceId UkmSourceID() const;

  // Tracks and reports UKM metrics of the number of attempted font family match
  // attempts (both successful and not successful) by the page.
  FontMatchingMetrics* GetFontMatchingMetrics();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType);

  void RecordUkmOutliveTimeAfterShutdown(int outlive_time_count);

  bool CurrentFrameHadRAF() const;
  bool NextFrameHasPendingRAF() const;

  const AtomicString& RequiredCSP();

  StylePropertyMapReadOnly* ComputedStyleMap(Element*);
  void AddComputedStyleMapItem(Element*, StylePropertyMapReadOnly*);
  StylePropertyMapReadOnly* RemoveComputedStyleMapItem(Element*);
  void NavigateLocalAdsFrames();

  SlotAssignmentEngine& GetSlotAssignmentEngine();

  bool IsSlotAssignmentOrLegacyDistributionDirty() const;

#if DCHECK_IS_ON()
  unsigned& SlotAssignmentRecalcForbiddenRecursionDepth() {
    return slot_assignment_recalc_forbidden_recursion_depth_;
  }
  bool IsSlotAssignmentRecalcForbidden() {
    return slot_assignment_recalc_forbidden_recursion_depth_ > 0;
  }
#else
  bool IsSlotAssignmentRecalcForbidden() { return false; }
#endif

  unsigned& FlatTreeTraversalForbiddenRecursionDepth() {
    return flat_tree_traversal_forbidden_recursion_depth_;
  }
  bool IsFlatTreeTraversalForbidden() {
    return flat_tree_traversal_forbidden_recursion_depth_ > 0;
  }

  unsigned& SlotAssignmentRecalcDepth() {
    return slot_assignment_recalc_depth_;
  }
  bool IsInSlotAssignmentRecalc() const {
    // Since we forbid recursive slot assignement recalc, the depth should be
    // <= 1.
    DCHECK_LE(slot_assignment_recalc_depth_, 1u);
    return slot_assignment_recalc_depth_ == 1;
  }

  bool IsVerticalScrollEnforced() const { return is_vertical_scroll_enforced_; }
  bool IsFocusAllowed() const;

  NavigationInitiatorImpl& NavigationInitiator();

  LazyLoadImageObserver& EnsureLazyLoadImageObserver();

  WindowAgent& GetWindowAgent();

  void IncrementNumberOfCanvases();

  void ProcessJavaScriptUrl(const KURL&, network::mojom::CSPDisposition);

  DisplayLockDocumentState& GetDisplayLockDocumentState() const;

  // Deferred compositor commits are disallowed by default, and are only allowed
  // for same-origin navigations to an html document fetched with http.
  bool DeferredCompositorCommitIsAllowed() {
    return deferred_compositor_commit_is_allowed_;
  }
  void SetDeferredCompositorCommitIsAllowed(bool new_value) {
    deferred_compositor_commit_is_allowed_ = new_value;
  }

  // Returns whether the document is inside the scope specified in the Web App
  // Manifest. If the document doesn't run in a context of a Web App or has no
  // associated Web App Manifest, it will return false.
  bool IsInWebAppScope() const;

  ComputedAccessibleNode* GetOrCreateComputedAccessibleNode(
      AXID ax_id,
      WebComputedAXTree* tree);

  bool HaveRenderBlockingResourcesLoaded() const;

  // Sets a beforeunload handler for documents which are embedding plugins. This
  // includes PluginDocument as well as an HTMLDocument which embeds a plugin
  // inside a cross-process frame (MimeHandlerView).
  void SetShowBeforeUnloadDialog(bool show_dialog);

  void ColorSchemeChanged();

  // A new vision deficiency is being emulated through DevTools.
  void VisionDeficiencyChanged();

  void ClearIsolatedWorldCSPForTesting(int32_t world_id);

  // A META element with name=color-scheme was added, removed, or modified.
  // Update the presentation level color-scheme property for the root element.
  void ColorSchemeMetaChanged();

  // Use counter related functions.
  void CountUse(mojom::WebFeature feature) final;
  void CountDeprecation(mojom::WebFeature feature) final;
  void CountUse(mojom::WebFeature feature) const;
  void CountProperty(CSSPropertyID property_id) const;
  void CountAnimatedProperty(CSSPropertyID property_id) const;
  // Count |feature| only when this document is associated with a cross-origin
  // iframe.
  void CountUseOnlyInCrossOriginIframe(mojom::WebFeature feature) const;
  // Return whether the Feature was previously counted for this document.
  // NOTE: only for use in testing.
  bool IsUseCounted(mojom::WebFeature) const;
  // Return whether the property was previously counted for this document.
  // NOTE: only for use in testing.
  bool IsPropertyCounted(CSSPropertyID property) const;
  // Return whether the animated property was previously counted for this
  // document.
  // NOTE: only for use in testing.
  bool IsAnimatedPropertyCounted(CSSPropertyID property) const;
  void ClearUseCounterForTesting(mojom::WebFeature);

  // Bind Content Security Policy to this document. This will cause the
  // CSP to resolve the 'self' attribute and all policies will then be
  // applied to this document.
  void BindContentSecurityPolicy();

  void UpdateForcedColors();
  bool InForcedColorsMode() const;

  // Capture the toggle event during parsing either by HTML parser or XML
  // parser.
  void SetToggleDuringParsing(bool toggle_during_parsing) {
    toggle_during_parsing_ = toggle_during_parsing;
  }
  bool ToggleDuringParsing() { return toggle_during_parsing_; }

  // We setup a dummy document to sanitize clipboard markup before pasting.
  // Sets and indicates whether this is the dummy document.
  void SetIsForMarkupSanitization(bool is_for_sanitization) {
    is_for_markup_sanitization_ = is_for_sanitization;
  }
  bool IsForMarkupSanitization() const { return is_for_markup_sanitization_; }

  bool HasPendingJavaScriptUrlsForTest() {
    return !pending_javascript_urls_.IsEmpty();
  }

  String GetFragmentDirective() const { return fragment_directive_; }
  bool UseCountFragmentDirective() const {
    return use_count_fragment_directive_;
  }

#if DCHECK_IS_ON()
  bool AllowDirtyShadowV0Traversal() const {
    return allow_dirty_shadow_v0_traversal_;
  }
  void SetAllowDirtyShadowV0Traversal(bool allow) {
    allow_dirty_shadow_v0_traversal_ = allow;
  }
#endif

  void ApplyScrollRestorationLogic();

  void MarkHasFindInPageRequest();
  void MarkHasFindInPageContentVisibilityActiveMatch();

  void CancelPendingJavaScriptUrls();

  HeapObserverList<SynchronousMutationObserver>&
  SynchronousMutationObserverList() {
    return synchronous_mutation_observer_list_;
  }

  void NotifyUpdateCharacterData(CharacterData* character_data,
                                 unsigned offset,
                                 unsigned old_length,
                                 unsigned new_length);
  void NotifyChangeChildren(const ContainerNode& container);

  FontPreloadManager& GetFontPreloadManager() { return font_preload_manager_; }
  void FontPreloadingFinishedOrTimedOut();

  void IncrementAsyncScriptCount() { async_script_count_++; }
  void RecordAsyncScriptCount();

  void MarkFirstPaint();
  void MaybeExecuteDelayedAsyncScripts();

  void SetFindInPageActiveMatchNode(Node*);
  const Node* GetFindInPageActiveMatchNode() const;

 protected:
  void ClearXMLVersion() { xml_version_ = String(); }

  virtual Document* CloneDocumentWithoutChildren() const;

  void LockCompatibilityMode() { compatibility_mode_locked_ = true; }
  ParserSynchronizationPolicy GetParserSynchronizationPolicy() const {
    return parser_sync_policy_;
  }

 private:
  friend class DocumentTest;
  friend class IgnoreDestructiveWriteCountIncrementer;
  friend class ThrowOnDynamicMarkupInsertionCountIncrementer;
  friend class IgnoreOpensDuringUnloadCountIncrementer;
  friend class NthIndexCache;
  FRIEND_TEST_ALL_PREFIXES(FrameFetchContextSubresourceFilterTest,
                           DuringOnFreeze);
  FRIEND_TEST_ALL_PREFIXES(DocumentTest, FindInPageUkm);
  class NetworkStateObserver;

  Document(const DocumentInit& initization,
           const SecurityContextInit& init_helper,
           DocumentClassFlags document_classes);

  // Post initialization of the object handling of both feature policy and
  // document policy.
  void PoliciesInitialized(const DocumentInit& document_initializer);

  friend class AXContext;
  void AddAXContext(AXContext*);
  void RemoveAXContext(AXContext*);

  bool IsDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  ScriptedIdleTaskController& EnsureScriptedIdleTaskController();
  void InitSecurityContext(const DocumentInit&);

  bool HasPendingVisualUpdate() const {
    return lifecycle_.GetState() == DocumentLifecycle::kVisualUpdatePending;
  }

  bool ShouldScheduleLayoutTreeUpdate() const;
  void ScheduleLayoutTreeUpdate();

  bool NeedsFullLayoutTreeUpdate() const;

  void PropagateStyleToViewport();

  void UpdateUseShadowTreesIfNeeded();
  void EvaluateMediaQueryListIfNeeded();

  void UpdateStyleInvalidationIfNeeded();
  void UpdateStyle();
  void NotifyLayoutTreeOfSubtreeChanges();
  bool ChildrenCanHaveStyle() const final;

  // ImplicitClose() actually does the work of closing the input stream.
  void ImplicitClose();
  bool ShouldComplete();

  // Returns |true| if both document and its owning frame are still attached.
  // Any of them could be detached during the check, e.g. by calling
  // iframe.remove() from an event handler.
  bool CheckCompletedInternal();

  void DetachParser();

  void BeginLifecycleUpdatesIfRenderingReady();

  void ChildrenChanged(const ChildrenChange&) override;

  String nodeName() const final;
  NodeType getNodeType() const final;
  bool ChildTypeAllowed(NodeType) const final;
  Node* Clone(Document&, CloneChildrenFlag) const override;
  void CloneDataFromDocument(const Document&);

  ShadowCascadeOrder shadow_cascade_order_ = kShadowCascadeNone;

  void UpdateTitle(const String&);
  void DispatchDidReceiveTitle();
  void UpdateFocusAppearance();
  void UpdateBaseURL();

  void ExecuteScriptsWaitingForResources();
  void ExecuteJavaScriptUrls();

  void LoadEventDelayTimerFired(TimerBase*);
  void PluginLoadingTimerFired(TimerBase*);

  void AddListenerType(ListenerType listener_type) {
    listener_types_ |= listener_type;
  }
  void AddMutationEventListenerTypeIfEnabled(ListenerType);

  void DidAssociateFormControlsTimerFired(TimerBase*);

  void ClearFocusedElementSoon();
  void ClearFocusedElementTimerFired(TimerBase*);
  bool HasNonEmptyFragment() const;

  bool HaveScriptBlockingStylesheetsLoaded() const;

  void SetHoverElement(Element*);

  using EventFactorySet = HashSet<std::unique_ptr<EventFactoryBase>>;
  static EventFactorySet& EventFactories();

  void SetNthIndexCache(NthIndexCache* nth_index_cache) {
    DCHECK(!nth_index_cache_ || !nth_index_cache);
    nth_index_cache_ = nth_index_cache;
  }

  const OriginAccessEntry& AccessEntryFromURL();

  void SendDidEditFieldInInsecureContext();

  bool HaveImportsLoaded() const;

  void UpdateActiveState(bool is_active, bool update_active_chain, Element*);
  void UpdateHoverState(Element*);

  const AtomicString& BodyAttributeValue(const QualifiedName&) const;
  void SetBodyAttribute(const QualifiedName&, const AtomicString&);

  const ParsedFeaturePolicy GetOwnerContainerPolicy() const;
  const FeaturePolicy* GetParentFeaturePolicy() const;

  // Returns true if use of |method_name| for markup insertion is allowed by
  // feature policy; otherwise returns false and throws a DOM exception.
  bool AllowedToUseDynamicMarkUpInsertion(const char* method_name,
                                          ExceptionState&);

  void SetFreezingInProgress(bool is_freezing_in_progress) {
    is_freezing_in_progress_ = is_freezing_in_progress;
  }

  void NotifyFocusedElementChanged(Element* old_focused_element,
                                   Element* new_focused_element);
  void DisplayNoneChangedForFrame();

  // Handles a connection error to |has_trust_tokens_answerer_| by rejecting all
  // pending promises created by |hasTrustToken|.
  void HasTrustTokensAnswererConnectionError();

  DocumentLifecycle lifecycle_;

  bool evaluate_media_queries_on_style_recalc_;

  // If we do ignore the pending stylesheet count, then we need to add a boolean
  // to track that this happened so that we can do a full repaint when the
  // stylesheets do eventually load.
  PendingSheetLayout pending_sheet_layout_;

  Member<LocalDOMWindow> dom_window_;
  Member<HTMLImportsController> imports_controller_;

  SecurityContext security_context_;

  // Document::CountUse() attributes the feature counts to the DocumentLoader
  // which is returned by Loader(). During construction Loader() returns null,
  // so we use this UseCounter instead.
  // TODO(dgozman): we should probably explicitly set and clear loader instead.
  Member<UseCounter> use_counter_during_construction_;

  // For Documents given a dom_window_ at creation that are not Shutdown(),
  // execution_context_ and dom_window_ will be equal.
  // For Documents given a dom_window_ at creation that are Shutdown(),
  // execution_context_ will be nullptr.
  // For Documents not given a dom_window_ at creation, execution_context_
  // will be the LocalDOMWindow where script will execute (which may be nullptr
  // in unit tests).
  Member<ExecutionContext> execution_context_;

  Member<ResourceFetcher> fetcher_;
  Member<DocumentParser> parser_;
  Member<ContextFeatures> context_features_;
  Member<HttpRefreshScheduler> http_refresh_scheduler_;

  bool well_formed_;

  // Document URLs.
  KURL url_;  // Document.URL: The URL from which this document was retrieved.
  KURL base_url_;  // Node.baseURI: The URL to use when resolving relative URLs.
  KURL base_url_override_;  // An alternative base URL that takes precedence
                            // over base_url_ (but not base_element_url_).
  KURL base_element_url_;   // The URL set by the <base> element.
  KURL cookie_url_;         // The URL to use for cookie access.
  std::unique_ptr<OriginAccessEntry> access_entry_from_url_;

  KURL web_bundle_claimed_url_;

  AtomicString base_target_;

  // Mime-type of the document in case it was cloned or created by XHR.
  AtomicString mime_type_;

  Member<DocumentType> doc_type_;
  Member<DOMImplementation> implementation_;

  Member<CSSStyleSheet> elem_sheet_;

  PrintingState printing_;
  bool is_painting_preview_;

  CompatibilityMode compatibility_mode_;
  // This is cheaper than making setCompatibilityMode virtual.
  bool compatibility_mode_locked_;

  TaskHandle execute_scripts_waiting_for_resources_task_handle_;
  TaskHandle javascript_url_task_handle_;
  struct PendingJavascriptUrl {
   public:
    PendingJavascriptUrl(const KURL& input_url,
                         network::mojom::CSPDisposition input_disposition)
        : url(input_url), disposition(input_disposition) {}
    KURL url;
    network::mojom::CSPDisposition disposition;
  };
  Vector<PendingJavascriptUrl> pending_javascript_urls_;

  // https://html.spec.whatwg.org/C/#autofocus-processed-flag
  bool autofocus_processed_flag_ = false;
  mojom::blink::FocusType last_focus_type_;
  bool had_keyboard_event_;
  TaskRunnerTimer<Document> clear_focused_element_timer_;
  // https://html.spec.whatwg.org/C/#autofocus-candidates
  // We implement this as a Vector because its maximum size is typically 1.
  HeapVector<Member<Element>> autofocus_candidates_;
  Member<Element> focused_element_;
  Member<Range> sequential_focus_navigation_starting_point_;
  Member<Element> hover_element_;
  Member<Element> active_element_;
  Member<Element> document_element_;
  UserActionElementSet user_action_elements_;
  Member<RootScrollerController> root_scroller_controller_;

  double overscroll_accumulated_delta_x_ = 0;
  double overscroll_accumulated_delta_y_ = 0;

  uint64_t dom_tree_version_;
  static uint64_t global_tree_version_;

  uint64_t style_version_;

  HeapHashSet<WeakMember<NodeIterator>> node_iterators_;
  using AttachedRangeSet = HeapHashSet<WeakMember<Range>>;
  AttachedRangeSet ranges_;

  uint16_t listener_types_;

  MutationObserverOptions mutation_observer_types_;

  Member<ElementIntersectionObserverData>
      document_explicit_root_intersection_observer_data_;

  Member<StyleEngine> style_engine_;
  Member<StyleSheetList> style_sheet_list_;

  Member<FormController> form_controller_;

  TextLinkColors text_link_colors_;
  const Member<VisitedLinkState> visited_link_state_;

  bool visually_ordered_;

  using ElementComputedStyleMap =
      HeapHashMap<WeakMember<Element>, Member<StylePropertyMapReadOnly>>;
  ElementComputedStyleMap element_computed_style_map_;

  DocumentReadyState ready_state_;
  ParsingState parsing_state_;

  bool is_dns_prefetch_enabled_;
  bool have_explicitly_disabled_dns_prefetch_;
  bool contains_plugins_;

  // https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#ignore-destructive-writes-counter
  unsigned ignore_destructive_write_count_;
  // https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#throw-on-dynamic-markup-insertion-counter
  unsigned throw_on_dynamic_markup_insertion_count_;
  // https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#ignore-opens-during-unload-counter
  unsigned ignore_opens_during_unload_count_;

  bool ignore_opens_and_writes_for_abort_ = false;

  String title_;
  String raw_title_;
  Member<Element> title_element_;

  Vector<AXContext*> ax_contexts_;
  Member<AXObjectCache> ax_object_cache_;
  Member<DocumentMarkerController> markers_;

  bool update_focus_appearance_after_layout_ = false;

  Member<Element> css_target_;

  bool was_discarded_;

  LoadEventProgress load_event_progress_;

  bool is_freezing_in_progress_;

  base::ElapsedTimer start_time_;

  Member<ScriptRunner> script_runner_;

  HeapVector<Member<ScriptElementBase>> current_script_stack_;

  std::unique_ptr<TransformSource> transform_source_;

  String xml_encoding_;
  String xml_version_;
  unsigned xml_standalone_ : 2;
  unsigned has_xml_declaration_ : 1;

  AtomicString content_language_;

  DocumentEncodingData encoding_data_;

  bool design_mode_;
  bool is_running_exec_command_;

  HeapHashSet<WeakMember<const LiveNodeListBase>>
      lists_invalidated_at_document_;
  LiveNodeListRegistry node_lists_;

  Member<SVGDocumentExtensions> svg_extensions_;

  Vector<AnnotatedRegionValue> annotated_regions_;
  bool has_annotated_regions_;
  bool annotated_regions_dirty_;

  std::unique_ptr<SelectorQueryCache> selector_query_cache_;

  // It is safe to keep a raw, untraced pointer to this stack-allocated
  // cache object: it is set upon the cache object being allocated on
  // the stack and cleared upon leaving its allocated scope. Hence it
  // is acceptable not to trace it -- should a conservative GC occur,
  // the cache object's references will be traced by a stack walk.
  GC_PLUGIN_IGNORE("461878")
  NthIndexCache* nth_index_cache_ = nullptr;

  DocumentClassFlags document_classes_;

  bool is_view_source_;
  bool is_xr_overlay_;
  bool saw_elements_in_known_namespaces_;
  bool is_srcdoc_document_;
  bool is_mobile_document_;

  LayoutView* layout_view_;

  // The last element in |top_layer_elements_| is topmost in the top layer
  // stack and is thus the one that will be visually on top.
  HeapVector<Member<Element>> top_layer_elements_;

  int load_event_delay_count_;
  TaskRunnerTimer<Document> load_event_delay_timer_;
  TaskRunnerTimer<Document> plugin_loading_timer_;

  DocumentTiming document_timing_;
  Member<MediaQueryMatcher> media_query_matcher_;
  bool write_recursion_is_too_deep_;
  unsigned write_recursion_depth_;

  Member<ScriptedAnimationController> scripted_animation_controller_;
  bool current_frame_is_throttled_;
  Member<ScriptedIdleTaskController> scripted_idle_task_controller_;
  Member<TextAutosizer> text_autosizer_;

  Member<V0CustomElementRegistrationContext> registration_context_;
  Member<V0CustomElementMicrotaskRunQueue> custom_element_microtask_run_queue_;

  void ElementDataCacheClearTimerFired(TimerBase*);
  TaskRunnerTimer<Document> element_data_cache_clear_timer_;

  Member<ElementDataCache> element_data_cache_;

  using LocaleIdentifierToLocaleMap =
      HashMap<AtomicString, std::unique_ptr<Locale>>;
  LocaleIdentifierToLocaleMap locale_cache_;

  Member<DocumentAnimations> document_animations_;
  Member<DocumentTimeline> timeline_;
  Member<PendingAnimations> pending_animations_;
  Member<WorkletAnimationController> worklet_animation_controller_;

  Member<Document> template_document_;
  Member<Document> template_document_host_;

  TaskRunnerTimer<Document> did_associate_form_controls_timer_;

  HeapHashSet<Member<SVGUseElement>> use_elements_needing_update_;

  bool has_viewport_units_;

  ParserSynchronizationPolicy parser_sync_policy_;

  Member<CanvasFontCache> canvas_font_cache_;

  Member<IntersectionObserverController> intersection_observer_controller_;

  int node_count_;

  bool may_contain_v0_shadow_ = false;

  Member<SnapCoordinator> snap_coordinator_;

  Member<PropertyRegistry> property_registry_;

  bool logged_field_edit_;

  TaskHandle sensitive_input_edited_task_;

  Member<NetworkStateObserver> network_state_observer_;

  std::unique_ptr<DocumentOutliveTimeReporter> document_outlive_time_reporter_;

  // |ukm_recorder_| and |source_id_| will allow objects that are part of
  // the document to record UKM.
  std::unique_ptr<ukm::UkmRecorder> ukm_recorder_;
  int64_t ukm_source_id_;
  bool needs_to_record_ukm_outlive_time_;

  // Tracks and reports UKM metrics of the number of attempted font family match
  // attempts (both successful and not successful) by the page.
  std::unique_ptr<FontMatchingMetrics> font_matching_metrics_;

#if DCHECK_IS_ON()
  unsigned slot_assignment_recalc_forbidden_recursion_depth_ = 0;
#endif
  unsigned slot_assignment_recalc_depth_ = 0;
  unsigned flat_tree_traversal_forbidden_recursion_depth_ = 0;

  Member<DOMFeaturePolicy> policy_;

  Member<SlotAssignmentEngine> slot_assignment_engine_;

  // TODO(tkent): Should it be moved to LocalFrame or LocalFrameView?
  Member<ViewportData> viewport_data_;

  // This is set through feature policy 'vertical-scroll'.
  bool is_vertical_scroll_enforced_ = false;

  // The number of canvas elements on the document
  int num_canvases_ = 0;

  // The number of LayoutObject::UpdateLayout() calls for both of the legacy
  // layout and LayoutNG.
  uint32_t layout_calls_counter_ = 0;

  // The number of LayoutObject::UpdateLayout() calls for LayoutNG.
  uint32_t layout_calls_counter_ng_ = 0;

  // The number of LayoutBlock instances for both of the legacy layout
  // and LayoutNG.
  uint32_t layout_blocks_counter_ = 0;

  // The number of LayoutNGMixin<LayoutBlock> instances
  uint32_t layout_blocks_counter_ng_ = 0;

  bool deferred_compositor_commit_is_allowed_ = false;

  // True when the document was created (in DomImplementation) for specific MIME
  // types that are handled externally. The document in this case is the
  // counterpart to a PluginDocument except that it contains a FrameView as
  // opposed to a PluginView.
  bool is_for_external_handler_;

#if DCHECK_IS_ON()
  // Allow traversal of Shadow DOM V0 traversal with dirty distribution.
  // Required for marking ancestors style-child-dirty.
  bool allow_dirty_shadow_v0_traversal_ = false;
#endif

  Member<NavigationInitiatorImpl> navigation_initiator_;
  Member<LazyLoadImageObserver> lazy_load_image_observer_;

  // Pending feature policy headers to send to browser after DidCommitNavigation
  // IPC.
  ParsedFeaturePolicy pending_fp_headers_;

  // Pending document policy headers to send to browser after
  // DidCommitNavigation IPC. Note: pending_dp_headers is the document policy
  // state used to initialize |document_policy_| in SecurityContext. Verifying
  // its integrity against required_document_policy has already been done in
  // DocumentLoader.
  DocumentPolicy::FeatureState pending_dp_headers_;

  // Tracks which feature policies have already been parsed, so as not to count
  // them multiple times.
  // The size of this vector is 0 until FeaturePolicyFeatureObserved is called.
  Vector<bool> parsed_feature_policies_;

  Vector<bool> parsed_document_policies_;

  AtomicString override_last_modified_;

  // Map from isolated world IDs to their ContentSecurityPolicy instances.
  Member<HeapHashMap<int, Member<ContentSecurityPolicy>>>
      isolated_world_csp_map_;

  // Used to keep track of which ComputedAccessibleNodes have already been
  // instantiated in this document to avoid constructing duplicates.
  HeapHashMap<AXID, Member<ComputedAccessibleNode>> computed_node_mapping_;

  // When the document contains MimeHandlerView, this variable might hold a
  // beforeunload handler. This will be set by the blink embedder when
  // necessary.
  Member<BeforeUnloadEventListener>
      mime_handler_view_before_unload_event_listener_;

  // Used to communicate state associated with resource management to the
  // embedder.
  std::unique_ptr<DocumentResourceCoordinator> resource_coordinator_;

  // Used for document.cookie. May be null.
  Member<CookieJar> cookie_jar_;

  bool toggle_during_parsing_ = false;

  bool is_for_markup_sanitization_ = false;

  String fragment_directive_;

  bool use_count_fragment_directive_ = false;

  HeapHashMap<WeakMember<Element>, Member<ExplicitlySetAttrElementsMap>>
      element_explicitly_set_attr_elements_map_;

  HeapObserverList<SynchronousMutationObserver>
      synchronous_mutation_observer_list_;

  Member<DisplayLockDocumentState> display_lock_document_state_;

  bool in_forced_colors_mode_;

  bool applying_scroll_restoration_logic_ = false;

  // Records find-in-page metrics, which are sent to UKM on shutdown.
  bool had_find_in_page_request_ = false;
  bool had_find_in_page_render_subtree_active_match_ = false;

  // To reduce the API noisiness an explicit deny decision will set a
  // flag that auto rejects the promise without the need for an IPC
  // call or potential user prompt.
  bool expressly_denied_storage_access_ = false;

  // Mojo remote used to determine if the document has permission to access
  // storage or not.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  // Mojo remote used to answer API calls asking whether the user has trust
  // tokens (https://github.com/wicg/trust-token-api). The other endpoint
  // is in the network service, which may crash and restart. To handle this:
  //   1. |pending_has_trust_tokens_resolvers_| keeps track of promises
  // depending on |has_trust_tokens_answerer_|'s answers;
  //   2. |HasTrustTokensAnswererConnectionError| handles connection errors by
  // rejecting all pending promises and clearing the pending set.
  HeapMojoRemote<network::mojom::blink::HasTrustTokensAnswerer>
      has_trust_tokens_answerer_;

  // In order to be able to answer promises when the Mojo remote disconnects,
  // maintain all pending promises here, deleting them on successful completion
  // or on connection error, whichever comes first.
  HeapHashSet<Member<ScriptPromiseResolver>>
      pending_has_trust_tokens_resolvers_;

  FontPreloadManager font_preload_manager_;

  int async_script_count_ = 0;
  bool first_paint_recorded_ = false;

  WeakMember<Node> find_in_page_active_match_node_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Document>;

inline void Document::ScheduleLayoutTreeUpdateIfNeeded() {
  // Inline early out to avoid the function calls below.
  if (HasPendingVisualUpdate())
    return;
  if (ShouldScheduleLayoutTreeUpdate() && NeedsLayoutTreeUpdate())
    ScheduleLayoutTreeUpdate();
}

#define DEFINE_DOCUMENT_TYPE_CASTS(thisType)                                \
  DEFINE_TYPE_CASTS(thisType, Document, document, document->Is##thisType(), \
                    document.Is##thisType())

// This is needed to avoid ambiguous overloads with the Node and TreeScope
// versions.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Document)

// Put these methods here, because they require the Document definition, but we
// really want to inline them.

inline bool Node::IsDocumentNode() const {
  return this == GetDocument();
}

Node* EventTargetNodeForDocument(Document*);

template <>
struct DowncastTraits<Document> {
  static bool AllowFrom(const Node& node) { return node.IsDocumentNode(); }
};

}  // namespace blink

#ifndef NDEBUG
// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showLiveDocumentInstances();
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_H_
