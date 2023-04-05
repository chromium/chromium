// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_extension_session_manager.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/fake_host_extension.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class HostExtensionSessionManagerTest : public testing::Test {
 public:
  HostExtensionSessionManagerTest()
      : extension1_("ext1", "cap1"),
        extension2_("ext2", std::string()),
        extension3_("ext3", "cap3") {
    extensions_.push_back(&extension1_);
    extensions_.push_back(&extension2_);
    extensions_.push_back(&extension3_);
  }

  HostExtensionSessionManagerTest(const HostExtensionSessionManagerTest&) =
      delete;
  HostExtensionSessionManagerTest& operator=(
      const HostExtensionSessionManagerTest&) = delete;

  ~HostExtensionSessionManagerTest() override = default;

 protected:
  // Fake HostExtensions for testing.
  FakeExtension extension1_;
  FakeExtension extension2_;
  FakeExtension extension3_;
  HostExtensionSessionManager::HostExtensions extensions_;

  // Mocks of interfaces provided by ClientSession.
  MockClientSessionDetails client_session_details_;
  protocol::MockClientStub client_stub_;
};

// Verifies that messages are handled by the correct extension.
TEST_F(HostExtensionSessionManagerTest, ExtensionMessages_MessageHandled) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  extension_manager.OnNegotiatedCapabilities(
      &client_stub_, extension_manager.GetCapabilities());

  protocol::ExtensionMessage message;
  message.set_type("ext2");
  extension_manager.OnExtensionMessage(message);

  EXPECT_FALSE(extension1_.has_handled_message());
  EXPECT_TRUE(extension2_.has_handled_message());
  EXPECT_FALSE(extension3_.has_handled_message());
}

// Verifies that extension messages not handled by extensions don't result in a
// crash.
TEST_F(HostExtensionSessionManagerTest, ExtensionMessages_MessageNotHandled) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  extension_manager.OnNegotiatedCapabilities(
      &client_stub_, extension_manager.GetCapabilities());

  protocol::ExtensionMessage message;
  message.set_type("ext4");
  extension_manager.OnExtensionMessage(message);

  EXPECT_FALSE(extension1_.has_handled_message());
  EXPECT_FALSE(extension2_.has_handled_message());
  EXPECT_FALSE(extension3_.has_handled_message());
}

// Verifies that the correct set of capabilities are reported to the client,
// based on the registered extensions.
TEST_F(HostExtensionSessionManagerTest, ExtensionCapabilities_AreReported) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);

  std::vector<std::string> reported_caps =
      base::SplitString(extension_manager.GetCapabilities(), " ",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::sort(reported_caps.begin(), reported_caps.end());

  ASSERT_EQ(2U, reported_caps.size());
  EXPECT_EQ("cap1", reported_caps[0]);
  EXPECT_EQ("cap3", reported_caps[1]);
}

// Verifies that an extension is not instantiated if the client does not
// support its required capability, and that it does not receive messages.
TEST_F(HostExtensionSessionManagerTest, ExtensionCapabilities_AreChecked) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  extension_manager.OnNegotiatedCapabilities(&client_stub_, "cap1");

  protocol::ExtensionMessage message;
  message.set_type("ext3");
  extension_manager.OnExtensionMessage(message);

  EXPECT_TRUE(extension1_.was_instantiated());
  EXPECT_TRUE(extension2_.was_instantiated());
  EXPECT_FALSE(extension3_.was_instantiated());
}

TEST_F(HostExtensionSessionManagerTest,
       FindExtensionSession_ReturnsSessionIfFound) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  extension_manager.OnNegotiatedCapabilities(&client_stub_, "cap1");
  EXPECT_EQ(extension1_.extension_session(),
            extension_manager.FindExtensionSession("cap1"));
}

TEST_F(HostExtensionSessionManagerTest,
       FindExtensionSession_ReturnsNullptrIfNegotiationHasNotCompleted) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  EXPECT_EQ(nullptr, extension_manager.FindExtensionSession("cap1"));
}

TEST_F(HostExtensionSessionManagerTest,
       FindExtensionSession_ReturnsNullptrIfCapabilityIsNotSupported) {
  HostExtensionSessionManager extension_manager(extensions_,
                                                &client_session_details_);
  extension_manager.OnNegotiatedCapabilities(&client_stub_, "cap1");
  EXPECT_EQ(nullptr, extension_manager.FindExtensionSession("cap2"));
}

}  // namespace remoting
