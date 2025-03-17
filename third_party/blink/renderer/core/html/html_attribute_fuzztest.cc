// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/fuzztest/src/fuzztest/googletest_fixture_adapter.h"

namespace blink {

namespace {

class BlinkSetupFixture : public RenderingTest {
 public:
  BlinkSetupFixture()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    SecurityContext& security_context =
        GetDocument().GetFrame()->DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://fuzztestorigin.test"));
  }
};

class BlinkAttributeFuzzTestFixture
    : public fuzztest::PerFuzzTestFixtureAdapter<BlinkSetupFixture> {
 public:
  void NoElementAttributeCrashes(html_names::HTMLTag tag,
                                 const std::string& attribute_name,
                                 const std::string& attribute_value) {
    Document& doc = GetDocument();
    Element* element =
        doc.CreateRawElement(html_names::TagToQualifiedName(tag));

    DummyExceptionStateForTesting exception_state;

    element->setAttribute(AtomicString(attribute_name.c_str()),
                          AtomicString(attribute_value.c_str()),
                          exception_state);
    doc.body()->AppendChild(element);
    doc.body()->RemoveChildren();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }
};

// A domain which returns an integer within the total number of known
// HTML elements, and can index into the html_names::HTMLTag array.
// This is not quite the optimal choice: ideally, we'd have a domain
// which returns the string of the tag, such that test cases are stable
// across Chromium versions when tags are added or removed. However, this
// proves to be difficult because all the Blink string types require
// garbage collection/allocation stuff to be initialized, and it isn't
// at the time that this domain function is run. If this fuzzer proves
// useful, though, we could expose more APIs from html_names.h.
auto AnyKnownTag() {
  return fuzztest::Map(
      [](int tag_id) { return static_cast<html_names::HTMLTag>(tag_id); },
      fuzztest::InRange(0U, html_names::kTagsCount - 1));
}

FUZZ_TEST_F(BlinkAttributeFuzzTestFixture, NoElementAttributeCrashes)
    .WithDomains(AnyKnownTag(),
                 // A note on the choice of domains for attribute names
                 // and values.
                 // This fuzzer would be quicker if we exposed a table
                 // from html_names.h for all known valid attribute names.
                 // But it might not actually explore parsing code which
                 // chokes on invalid attribute names. Similarly,
                 // we could use domains for any string rather than
                 // just printable strings, to even more thoroughly explore
                 // the parsing code. But this seems a good compromise.
                 fuzztest::PrintableAsciiString(),
                 fuzztest::PrintableAsciiString());

}  // namespace

}  // namespace blink
