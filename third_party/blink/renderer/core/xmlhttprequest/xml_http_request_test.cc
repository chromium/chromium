// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

class XMLHttpRequestTest : public PageTestBase {
 protected:
};

// An XHR with an origin with `CanLoadLocalResources` set cannot set forbidden
// request headers. It was historically allowed, and this is a regression test.
// See https://crbug.com/567527 for details.
TEST_F(XMLHttpRequestTest, ForbiddenRequestHeaderWithLocalOrigin) {
  GetFrame().DomWindow()->GetMutableSecurityOrigin()->GrantLoadLocalResources();

  auto* xhr = XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));

  xhr->open(http_names::kGET, "https://example.com/", ASSERT_NO_EXCEPTION);
  xhr->setRequestHeader(AtomicString("host"), AtomicString("example.com"),
                        ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(xhr->HasRequestHeaderForTesting(AtomicString("host")));
}

}  // namespace
}  // namespace blink
