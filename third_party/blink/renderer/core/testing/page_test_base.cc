// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/page_test_base.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/core/css/font_face_descriptors.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/loader/fetch/substitute_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

PageTestBase::PageTestBase() = default;

PageTestBase::~PageTestBase() = default;

void PageTestBase::SetUp() {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));

  // Use no-quirks (ake "strict") mode by default.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  // Use desktop page scale limits by default.
  GetPage().SetDefaultPageScaleLimits(1, 4);
}

void PageTestBase::SetUp(IntSize size) {
  DCHECK(!dummy_page_holder_) << "Page should be set up only once";
  dummy_page_holder_ = DummyPageHolder::Create(size);

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
  dummy_page_holder_ = DummyPageHolder::Create(
      IntSize(800, 600), clients, local_frame_client, setting_overrider);

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
  FontFace* ahem =
      FontFace::Create(&document, "Ahem", buffer, FontFaceDescriptors());

  ScriptState* script_state = ToScriptStateForMainWorld(&frame);
  DummyExceptionStateForTesting exception_state;
  FontFaceSetDocument::From(document)->addForBinding(script_state, ahem,
                                                     exception_state);
}

// Both sets the inner html and runs the document lifecycle.
void PageTestBase::SetBodyInnerHTML(const String& body_content) {
  GetDocument().body()->SetInnerHTMLFromString(body_content,
                                               ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();
}

void PageTestBase::SetBodyContent(const std::string& body_content) {
  SetBodyInnerHTML(String::FromUTF8(body_content.c_str()));
}

void PageTestBase::SetHtmlInnerHTML(const std::string& html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content.c_str()));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

void PageTestBase::UpdateAllLifecyclePhases() {
  GetDocument().View()->UpdateAllLifecyclePhases();
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

}  // namespace blink
