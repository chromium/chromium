// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/page_test_base.h"

#include "base/test/bind_test_util.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/core/css/font_face_descriptors.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

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

}  // namespace

PageTestBase::PageTestBase() = default;

PageTestBase::~PageTestBase() = default;

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
      IntSize(800, 600), nullptr, nullptr, std::move(setter), GetTickClock());

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);
}

void PageTestBase::SetUp(IntSize size) {
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
}

void PageTestBase::SetupPageWithClients(
    Page::PageClients* clients,
    LocalFrameClient* local_frame_client,
    FrameSettingOverrideFunction setting_overrider) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  auto setter = base::BindLambdaForTesting([&](Settings& settings) {
    if (setting_overrider)
      setting_overrider(settings);
    if (enable_compositing_)
      settings.SetAcceleratedCompositingEnabled(true);
  });
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), clients, local_frame_client, std::move(setter),
      GetTickClock());

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);
}

void PageTestBase::TearDown() {
  dummy_page_holder_ = nullptr;
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
  Document& document = *frame.DomWindow()->document();
  scoped_refptr<SharedBuffer> shared_buffer =
      test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
  StringOrArrayBufferOrArrayBufferView buffer =
      StringOrArrayBufferOrArrayBufferView::FromArrayBuffer(
          DOMArrayBuffer::Create(shared_buffer));
  FontFace* ahem = FontFace::Create(&document, "Ahem", buffer,
                                    FontFaceDescriptors::Create());

  ScriptState* script_state = ToScriptStateForMainWorld(&frame);
  DummyExceptionStateForTesting exception_state;
  FontFaceSetDocument::From(document)->addForBinding(script_state, ahem,
                                                     exception_state);
}

// Both sets the inner html and runs the document lifecycle.
void PageTestBase::SetBodyInnerHTML(const String& body_content) {
  GetDocument().body()->SetInnerHTMLFromString(body_content,
                                               ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
}

void PageTestBase::SetBodyContent(const std::string& body_content) {
  SetBodyInnerHTML(String::FromUTF8(body_content));
}

void PageTestBase::SetHtmlInnerHTML(const std::string& html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content));
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
                              const String& feature_policy_header,
                              const String& csp_header) {
  auto params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url);
  if (!feature_policy_header.IsEmpty()) {
    params->response.SetHttpHeaderField(http_names::kFeaturePolicy,
                                        feature_policy_header);
  }
  if (!csp_header.IsEmpty()) {
    params->response.SetHttpHeaderField(http_names::kContentSecurityPolicy,
                                        csp_header);
  }
  GetFrame().Loader().CommitNavigation(std::move(params),
                                       nullptr /* extra_data */);

  blink::test::RunPendingTasks();
  ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
}

void PageTestBase::UpdateAllLifecyclePhasesForTest() {
  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  GetDocument().View()->RunPostLifecycleSteps();
}

StyleEngine& PageTestBase::GetStyleEngine() {
  return GetDocument().GetStyleEngine();
}

Element* PageTestBase::GetElementById(const char* id) const {
  return GetDocument().getElementById(id);
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
  platform_ = std::make_unique<
      ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>>();
}

const base::TickClock* PageTestBase::GetTickClock() {
  return platform_ ? platform()->test_task_runner()->GetMockTickClock()
                   : base::DefaultTickClock::GetInstance();
}

}  // namespace blink
