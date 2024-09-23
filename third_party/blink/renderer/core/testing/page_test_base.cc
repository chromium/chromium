// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/page_test_base.h"

#include <sstream>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

Element* GetOrCreateElement(ContainerNode* parent,
                            const HTMLQualifiedName& tag_name) {
  HTMLCollection* elements = parent->getElementsByTagNameNS(
      tag_name.NamespaceURI(), tag_name.LocalName());
  if (!elements->IsEmpty())
    return elements->item(0);
  return parent->ownerDocument()->CreateRawElement(
      tag_name, CreateElementFlags::ByCreateElement());
}

void ToSimpleLayoutTree(std::ostream& ostream,
                        const LayoutObject& layout_object,
                        int depth) {
  for (int i = 1; i < depth; ++i)
    ostream << "|  ";
  ostream << (depth ? "+--" : "") << layout_object.GetName() << " ";
  if (auto* node = layout_object.GetNode())
    ostream << *node;
  else
    ostream << "(anonymous)";
  if (auto* layout_text_fragment =
          DynamicTo<LayoutTextFragment>(layout_object)) {
    ostream << " (" << layout_text_fragment->TransformedText() << ")";
  } else if (auto* layout_text = DynamicTo<LayoutText>(layout_object)) {
    if (!layout_object.GetNode())
      ostream << " " << layout_text->TransformedText();
  }
  ostream << std::endl;
  for (auto* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    ostream << "  ";
    ToSimpleLayoutTree(ostream, *child, depth + 1);
  }
}

}  // namespace

PageTestBase::MockClipboardHostProvider::MockClipboardHostProvider(
    const blink::BrowserInterfaceBrokerProxy& interface_broker) {
  Install(interface_broker);
}

PageTestBase::MockClipboardHostProvider::MockClipboardHostProvider() = default;

PageTestBase::MockClipboardHostProvider::~MockClipboardHostProvider() {
  if (interface_broker_) {
    interface_broker_->SetBinderForTesting(
        blink::mojom::blink::ClipboardHost::Name_, {});
  }
}

void PageTestBase::MockClipboardHostProvider::Install(
    const blink::BrowserInterfaceBrokerProxy& interface_broker) {
  interface_broker_ = &interface_broker;
  interface_broker_->SetBinderForTesting(
      blink::mojom::blink::ClipboardHost::Name_,
      WTF::BindRepeating(
          &PageTestBase::MockClipboardHostProvider::BindClipboardHost,
          base::Unretained(this)));
}

void PageTestBase::MockClipboardHostProvider::BindClipboardHost(
    mojo::ScopedMessagePipeHandle handle) {
  host_.Bind(mojo::PendingReceiver<blink::mojom::blink::ClipboardHost>(
      std::move(handle)));
}

PageTestBase::PageTestBase() = default;

PageTestBase::PageTestBase(base::test::TaskEnvironment::TimeSource time_source)
    : task_environment_(time_source) {}

PageTestBase::~PageTestBase() {
  dummy_page_holder_.reset();
  MemoryCache::Get()->EvictResources();
  // Clear lazily loaded style sheets.
  CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();
  // Run garbage collection before the task environment is destroyed so task
  // time observers shutdown during GC can unregister themselves.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

void PageTestBase::EnableCompositing() {
  DCHECK(!dummy_page_holder_)
      << "EnableCompositing() must be called before set up";
  enable_compositing_ = true;
}

void PageTestBase::SetUp() {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  auto setter = base::BindLambdaForTesting([&](Settings& settings) {
    if (enable_compositing_)
      settings.SetAcceleratedCompositingEnabled(true);
  });
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr, nullptr, std::move(setter), GetTickClock());

  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  mock_clipboard_host_provider_.Install(GetFrame().GetBrowserInterfaceBroker());

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);

  // We do a lot of one-offs in unit tests, so update this so that every
  // single test doesn't have to.
  GetStyleEngine().UpdateViewportSize();
}

void PageTestBase::SetUp(gfx::Size size) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  auto setter = base::BindLambdaForTesting([&](Settings& settings) {
    if (enable_compositing_)
      settings.SetAcceleratedCompositingEnabled(true);
  });
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(
      size, nullptr, nullptr, std::move(setter), GetTickClock());

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);

  // We do a lot of one-offs in unit tests, so update this so that every
  // single test doesn't have to.
  GetStyleEngine().UpdateViewportSize();
}

void PageTestBase::SetupPageWithClients(
    ChromeClient* chrome_client,
    LocalFrameClient* local_frame_client,
    FrameSettingOverrideFunction setting_overrider,
    gfx::Size size) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  auto setter = base::BindLambdaForTesting([&](Settings& settings) {
    if (setting_overrider)
      setting_overrider(settings);
    if (enable_compositing_)
      settings.SetAcceleratedCompositingEnabled(true);
  });

  dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(size, chrome_client, local_frame_client,
                                        std::move(setter), GetTickClock());

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);

  // We do a lot of one-offs in unit tests, so update this so that every
  // single test doesn't have to.
  GetStyleEngine().UpdateViewportSize();
}

void PageTestBase::TearDown() {
  dummy_page_holder_ = nullptr;
  MemoryCache::Get()->EvictResources();
}

Document& PageTestBase::GetDocument() const {
  return dummy_page_holder_->GetDocument();
}

Page& PageTestBase::GetPage() const {
  return dummy_page_holder_->GetPage();
}

LocalFrame& PageTestBase::GetFrame() const {
  return GetDummyPageHolder().GetFrame();
}

FrameSelection& PageTestBase::Selection() const {
  return GetFrame().Selection();
}

void PageTestBase::LoadAhem() {
  LoadAhem(GetFrame());
}

void PageTestBase::LoadAhem(LocalFrame& frame) {
  LoadFontFromFile(frame, test::CoreTestDataPath("Ahem.ttf"),
                   AtomicString("Ahem"));
}

void PageTestBase::LoadFontFromFile(LocalFrame& frame,
                                    String font_path,
                                    const AtomicString& family_name) {
  Document& document = *frame.DomWindow()->document();
  std::optional<Vector<char>> data = test::ReadFromFile(font_path);
  ASSERT_TRUE(data);
  auto* buffer =
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          DOMArrayBuffer::Create(base::as_byte_span(*data)));
  FontFace* ahem = FontFace::Create(frame.DomWindow(), family_name, buffer,
                                    FontFaceDescriptors::Create());

  ScriptState* script_state = ToScriptStateForMainWorld(&frame);
  DummyExceptionStateForTesting exception_state;
  FontFaceSetDocument::From(document)->addForBinding(script_state, ahem,
                                                     exception_state);
}

void PageTestBase::LoadNoto() {
  LoadNoto(GetFrame());
}

void PageTestBase::LoadNoto(LocalFrame& frame) {
  LoadFontFromFile(frame,
                   blink::test::PlatformTestDataPath(
                       "third_party/Noto/NotoNaskhArabic-regular.woff2"),
                   AtomicString("NotoArabic"));
}

// Both sets the inner html and runs the document lifecycle.
void PageTestBase::SetBodyInnerHTML(const String& body_content) {
  GetDocument().body()->setInnerHTML(body_content, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
}

void PageTestBase::SetBodyContent(const std::string& body_content) {
  SetBodyInnerHTML(String::FromUTF8(body_content));
}

void PageTestBase::SetHtmlInnerHTML(const std::string& html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

void PageTestBase::InsertStyleElement(const std::string& style_rules) {
  Element* const head =
      GetOrCreateElement(&GetDocument(), html_names::kHeadTag);
  DCHECK_EQ(head, GetOrCreateElement(&GetDocument(), html_names::kHeadTag));
  Element* const style = GetDocument().CreateRawElement(
      html_names::kStyleTag, CreateElementFlags::ByCreateElement());
  style->setTextContent(String(style_rules.data(), style_rules.size()));
  head->appendChild(style);
}

void PageTestBase::NavigateTo(const KURL& url,
                              const WTF::HashMap<String, String>& headers) {
  auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);

  for (const auto& header : headers)
    params->response.SetHttpHeaderField(header.key, header.value);

  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies(),
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());

  // Add parsed Content Security Policies to the policy container, simulating
  // what the browser does.
  for (auto& csp : ParseContentSecurityPolicies(
           params->response.HttpHeaderField("content-security-policy"),
           network::mojom::blink::ContentSecurityPolicyType::kEnforce,
           network::mojom::blink::ContentSecurityPolicySource::kHTTP, url)) {
    params->policy_container->policies.content_security_policies.emplace_back(
        ConvertToPublic(std::move(csp)));
  }

  GetFrame().Loader().CommitNavigation(std::move(params),
                                       nullptr /* extra_data */);

  blink::test::RunPendingTasks();
  ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
}

void PageTestBase::UpdateAllLifecyclePhasesForTest() {
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
}

StyleEngine& PageTestBase::GetStyleEngine() {
  return GetDocument().GetStyleEngine();
}

Element* PageTestBase::GetElementById(const char* id) const {
  return GetDocument().getElementById(AtomicString(id));
}

AnimationClock& PageTestBase::GetAnimationClock() {
  return GetDocument().GetAnimationClock();
}

PendingAnimations& PageTestBase::GetPendingAnimations() {
  return GetDocument().GetPendingAnimations();
}

FocusController& PageTestBase::GetFocusController() const {
  return GetDocument().GetPage()->GetFocusController();
}

void PageTestBase::EnablePlatform() {
  DCHECK(!platform_);
  platform_ =
      std::make_unique<ScopedTestingPlatformSupport<TestingPlatformSupport>>();
}

// See also LayoutTreeAsText to dump with geometry and paint layers.
// static
std::string PageTestBase::ToSimpleLayoutTree(
    const LayoutObject& layout_object) {
  std::ostringstream ostream;
  ostream << std::endl;
  ::blink::ToSimpleLayoutTree(ostream, layout_object, 0);
  return ostream.str();
}

void PageTestBase::SetPreferCompositingToLCDText(bool enable) {
  GetPage().GetSettings().SetPreferCompositingToLCDTextForTesting(enable);
}

const base::TickClock* PageTestBase::GetTickClock() {
  return base::DefaultTickClock::GetInstance();
}

void PageTestBase::FastForwardBy(base::TimeDelta delta) {
  return task_environment_.FastForwardBy(delta);
}

void PageTestBase::FastForwardUntilNoTasksRemain() {
  return task_environment_.FastForwardUntilNoTasksRemain();
}

void PageTestBase::AdvanceClock(base::TimeDelta delta) {
  return task_environment_.AdvanceClock(delta);
}

}  // namespace blink
