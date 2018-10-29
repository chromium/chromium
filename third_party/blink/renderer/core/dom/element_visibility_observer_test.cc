// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_visibility_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"

namespace blink {

namespace {

class ElementVisibilityObserverTest : public ::testing::Test {
 protected:
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(ElementVisibilityObserverTest, ObserveElementWithoutDocumentFrame) {
  helper_.Initialize();
  Document& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  HTMLElement* element = HTMLDivElement::Create(
      *DOMImplementation::Create(document)->createHTMLDocument("test"));
  ElementVisibilityObserver* observer = new ElementVisibilityObserver(
      element, ElementVisibilityObserver::VisibilityCallback());
  observer->Start();
  observer->Stop();
  // It should not crash.
}

TEST_F(ElementVisibilityObserverTest, ObserveElementWithRemoteFrameParent) {
  helper_.InitializeRemote();

  WebLocalFrameImpl* child_frame =
      frame_test_helpers::CreateLocalChild(*helper_.RemoteMainFrame());
  Document& document = *child_frame->GetFrame()->GetDocument();

  Persistent<HTMLElement> element = HTMLDivElement::Create(document);
  ElementVisibilityObserver* observer =
      new ElementVisibilityObserver(element, WTF::BindRepeating([](bool) {}));
  observer->Start();
  observer->DeliverObservationsForTesting();
  observer->Stop();
  // It should not crash.
}

}  // anonymous namespace

}  // blink namespace
