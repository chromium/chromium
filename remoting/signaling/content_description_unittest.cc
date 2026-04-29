// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/content_description.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

TEST(ContentDescriptionTest, FormatAndParse) {
  std::unique_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  ContentDescription description(std::move(config), JingleAuthentication());
  std::unique_ptr<jingle_xmpp::XmlElement> xml(
      ContentDescriptionToXml(description));
  std::unique_ptr<ContentDescription> parsed(
      ContentDescriptionFromXml(xml.get(), true));
  ASSERT_TRUE(parsed.get());
  EXPECT_TRUE(parsed->config()->webrtc_supported());
}

}  // namespace remoting
