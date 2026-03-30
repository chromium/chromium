// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"

namespace blink {

class UserMediaRequestProviderImplTest : public PageTestBase {};

// Verifies that the provider doesn't crash when attempting to process a request
// without a valid UserMediaClient.
TEST_F(UserMediaRequestProviderImplTest, StartRequestEarlyExitNoClient) {
  UserMediaRequestProviderImpl::ProvideTo(*GetDocument().domWindow());
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  provider->StartRequest(element, AtomicString("camera"));
  // Test passes if it doesn't crash.
}

// Verifies that StartRequest gracefully exits and makes no changes when an
// active stream is already present.
TEST_F(UserMediaRequestProviderImplTest, StartRequestActiveStreamExists) {
  UserMediaRequestProviderImpl::ProvideTo(*GetDocument().domWindow());
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  HTMLUserMediaElementMediaStream::From(*element).SetMediaStream(stream);

  provider->StartRequest(element, AtomicString("camera"));
  // Test passes if it doesn't crash and does not change the stream.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);
}

// Confirms that providing a valid stream sets the generated stream onto the
// HTMLUserMediaElementMediaStream successfully.
TEST_F(UserMediaRequestProviderImplTest, CallbacksOnSuccessWithStream) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), nullptr);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  MediaStreamVector streams = {stream};

  callbacks->OnSuccess(streams, /*capture_controller=*/nullptr);

  // The stream should have been set on the element.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);
}

}  // namespace blink
