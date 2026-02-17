// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_experiment_session_plugin.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_attributes.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(HostExperimentSessionPluginTest, AttachAttributes) {
  HostExperimentSessionPlugin plugin;
  std::optional<Attachment> attachment = plugin.GetNextMessage();
  ASSERT_TRUE(attachment);
  ASSERT_TRUE(attachment->host_attributes);
  EXPECT_THAT(attachment->host_attributes->attribute,
              testing::Contains(GetHostAttributes()));

  attachment = plugin.GetNextMessage();
  ASSERT_FALSE(attachment);
}

TEST(HostExperimentSessionPluginTest, LoadConfiguration) {
  Attachment attachment;
  HostConfigAttachment config;
  config.settings["Detect-Updated-Region"] = "test-value";
  config.settings["Exp1"] = "val1";
  config.settings["Exp2"] = "val2";
  attachment.host_config = std::move(config);

  HostExperimentSessionPlugin plugin;
  plugin.OnIncomingMessage(attachment);
  ASSERT_TRUE(plugin.configuration_received());

  const base::DictValue& configuration = plugin.configuration();
  EXPECT_EQ(configuration.size(), 3u);
  EXPECT_EQ(*configuration.FindString("Detect-Updated-Region"), "test-value");
  EXPECT_EQ(*configuration.FindString("Exp1"), "val1");
  EXPECT_EQ(*configuration.FindString("Exp2"), "val2");
}

TEST(HostExperimentSessionPluginTest, IgnoreSecondConfiguration) {
  Attachment attachment1;
  HostConfigAttachment config1;
  config1.settings["Detect-Updated-Region"] = "val1";
  attachment1.host_config = std::move(config1);

  HostExperimentSessionPlugin plugin;
  plugin.OnIncomingMessage(attachment1);
  ASSERT_TRUE(plugin.configuration_received());
  EXPECT_EQ(*plugin.configuration().FindString("Detect-Updated-Region"),
            "val1");

  Attachment attachment2;
  HostConfigAttachment config2;
  config2.settings["Detect-Updated-Region"] = "val2";
  attachment2.host_config = std::move(config2);
  plugin.OnIncomingMessage(attachment2);
  ASSERT_TRUE(plugin.configuration_received());
  EXPECT_EQ(*plugin.configuration().FindString("Detect-Updated-Region"),
            "val1");
}

}  // namespace remoting
