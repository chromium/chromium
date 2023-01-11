// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_experiment_session_plugin.h"

#include <memory>

#include "base/functional/bind.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_attributes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

TEST(HostExperimentSessionPluginTest, AttachAttributes) {
  HostExperimentSessionPlugin plugin;
  std::unique_ptr<XmlElement> attachments = plugin.GetNextMessage();
  ASSERT_TRUE(attachments);
  ASSERT_EQ(attachments->Name(),
            QName(kChromotingXmlNamespace, "host-attributes"));
  ASSERT_EQ(attachments->BodyText(), GetHostAttributes());

  attachments.reset();
  attachments = plugin.GetNextMessage();
  ASSERT_FALSE(attachments);
}

TEST(HostExperimentSessionPluginTest, LoadConfiguration) {
  std::unique_ptr<XmlElement> attachment(
      new XmlElement(QName(kChromotingXmlNamespace, "attachments")));
  XmlElement* configuration =
      new XmlElement(QName(kChromotingXmlNamespace, "host-configuration"));
  configuration->SetBodyText("This Is A Test Configuration");
  attachment->AddElement(configuration);
  HostExperimentSessionPlugin plugin;
  plugin.OnIncomingMessage(*attachment);
  ASSERT_TRUE(plugin.configuration_received());
  ASSERT_EQ(plugin.configuration(), "This Is A Test Configuration");
}

TEST(HostExperimentSessionPluginTest, IgnoreSecondConfiguration) {
  std::unique_ptr<XmlElement> attachment(
      new XmlElement(QName(kChromotingXmlNamespace, "attachments")));
  XmlElement* configuration =
      new XmlElement(QName(kChromotingXmlNamespace, "host-configuration"));
  attachment->AddElement(configuration);
  configuration->SetBodyText("config1");
  HostExperimentSessionPlugin plugin;
  plugin.OnIncomingMessage(*attachment);
  ASSERT_TRUE(plugin.configuration_received());
  ASSERT_EQ(plugin.configuration(), "config1");

  configuration->SetBodyText("config2");
  plugin.OnIncomingMessage(*attachment);
  ASSERT_TRUE(plugin.configuration_received());
  ASSERT_EQ(plugin.configuration(), "config1");
}

}  // namespace remoting
