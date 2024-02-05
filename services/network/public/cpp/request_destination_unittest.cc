// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/request_destination.h"

#include <string_view>

#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(RequestDestinationTest, ToStringUseTheEmptyString) {
  struct {
    mojom::RequestDestination request_destination;
    std::string_view expected_string;
  } kTestCases[] = {
      {mojom::RequestDestination::kEmpty, ""},
      {mojom::RequestDestination::kAudio, "audio"},
      {mojom::RequestDestination::kAudioWorklet, "audioworklet"},
      {mojom::RequestDestination::kDocument, "document"},
      {mojom::RequestDestination::kEmbed, "embed"},
      {mojom::RequestDestination::kFont, "font"},
      {mojom::RequestDestination::kFrame, "frame"},
      {mojom::RequestDestination::kIframe, "iframe"},
      {mojom::RequestDestination::kImage, "image"},
      {mojom::RequestDestination::kJson, "json"},
      {mojom::RequestDestination::kManifest, "manifest"},
      {mojom::RequestDestination::kObject, "object"},
      {mojom::RequestDestination::kPaintWorklet, "paintworklet"},
      {mojom::RequestDestination::kReport, "report"},
      {mojom::RequestDestination::kScript, "script"},
      {mojom::RequestDestination::kServiceWorker, "serviceworker"},
      {mojom::RequestDestination::kSharedWorker, "sharedworker"},
      {mojom::RequestDestination::kStyle, "style"},
      {mojom::RequestDestination::kTrack, "track"},
      {mojom::RequestDestination::kVideo, "video"},
      {mojom::RequestDestination::kWebBundle, "webbundle"},
      {mojom::RequestDestination::kWorker, "worker"},
      {mojom::RequestDestination::kXslt, "xslt"},
      {mojom::RequestDestination::kFencedframe, "fencedframe"},
      {mojom::RequestDestination::kWebIdentity, "webidentity"},
      {mojom::RequestDestination::kDictionary, "dictionary"},
      {mojom::RequestDestination::kSpeculationRules, "speculationrules"},
  };

  for (const auto& testcase : kTestCases) {
    SCOPED_TRACE(testcase.request_destination);
    EXPECT_EQ(testcase.expected_string,
              RequestDestinationToString(
                  testcase.request_destination,
                  EmptyRequestDestinationOption::kUseTheEmptyString));
  }
}

TEST(RequestDestinationTest, ToStringUseFiveCharEmptyString) {
  struct {
    mojom::RequestDestination request_destination;
    std::string_view expected_string;
  } kTestCases[] = {
      {mojom::RequestDestination::kEmpty, "empty"},
      {mojom::RequestDestination::kAudio, "audio"},
      {mojom::RequestDestination::kAudioWorklet, "audioworklet"},
      {mojom::RequestDestination::kDocument, "document"},
      {mojom::RequestDestination::kEmbed, "embed"},
      {mojom::RequestDestination::kFont, "font"},
      {mojom::RequestDestination::kFrame, "frame"},
      {mojom::RequestDestination::kIframe, "iframe"},
      {mojom::RequestDestination::kImage, "image"},
      {mojom::RequestDestination::kJson, "json"},
      {mojom::RequestDestination::kManifest, "manifest"},
      {mojom::RequestDestination::kObject, "object"},
      {mojom::RequestDestination::kPaintWorklet, "paintworklet"},
      {mojom::RequestDestination::kReport, "report"},
      {mojom::RequestDestination::kScript, "script"},
      {mojom::RequestDestination::kServiceWorker, "serviceworker"},
      {mojom::RequestDestination::kSharedWorker, "sharedworker"},
      {mojom::RequestDestination::kStyle, "style"},
      {mojom::RequestDestination::kTrack, "track"},
      {mojom::RequestDestination::kVideo, "video"},
      {mojom::RequestDestination::kWebBundle, "webbundle"},
      {mojom::RequestDestination::kWorker, "worker"},
      {mojom::RequestDestination::kXslt, "xslt"},
      {mojom::RequestDestination::kFencedframe, "fencedframe"},
      {mojom::RequestDestination::kWebIdentity, "webidentity"},
      {mojom::RequestDestination::kDictionary, "dictionary"},
      {mojom::RequestDestination::kSpeculationRules, "speculationrules"},
  };

  for (const auto& testcase : kTestCases) {
    SCOPED_TRACE(testcase.request_destination);
    EXPECT_EQ(testcase.expected_string,
              RequestDestinationToString(
                  testcase.request_destination,
                  EmptyRequestDestinationOption::kUseFiveCharEmptyString));
  }
}

TEST(RequestDestinationTest, FromStringUseTheEmptyString) {
  struct {
    std::string_view input;
    std::optional<mojom::RequestDestination> expected_destination;
  } kTestCases[] = {
      {"", mojom::RequestDestination::kEmpty},
      {"empty", std::nullopt},
      {"audio", mojom::RequestDestination::kAudio},
      {"audioworklet", mojom::RequestDestination::kAudioWorklet},
      {"document", mojom::RequestDestination::kDocument},
      {"embed", mojom::RequestDestination::kEmbed},
      {"font", mojom::RequestDestination::kFont},
      {"frame", mojom::RequestDestination::kFrame},
      {"iframe", mojom::RequestDestination::kIframe},
      {"image", mojom::RequestDestination::kImage},
      {"json", mojom::RequestDestination::kJson},
      {"manifest", mojom::RequestDestination::kManifest},
      {"object", mojom::RequestDestination::kObject},
      {"paintworklet", mojom::RequestDestination::kPaintWorklet},
      {"report", mojom::RequestDestination::kReport},
      {"script", mojom::RequestDestination::kScript},
      {"serviceworker", mojom::RequestDestination::kServiceWorker},
      {"sharedworker", mojom::RequestDestination::kSharedWorker},
      {"style", mojom::RequestDestination::kStyle},
      {"track", mojom::RequestDestination::kTrack},
      {"video", mojom::RequestDestination::kVideo},
      {"webbundle", mojom::RequestDestination::kWebBundle},
      {"worker", mojom::RequestDestination::kWorker},
      {"xslt", mojom::RequestDestination::kXslt},
      {"fencedframe", mojom::RequestDestination::kFencedframe},
      {"webidentity", mojom::RequestDestination::kWebIdentity},
      {"dictionary", mojom::RequestDestination::kDictionary},
      {"speculationrules", mojom::RequestDestination::kSpeculationRules},

      {"unknown", std::nullopt},
  };
  for (const auto& testcase : kTestCases) {
    SCOPED_TRACE(testcase.input);
    EXPECT_EQ(
        testcase.expected_destination,
        RequestDestinationFromString(
            testcase.input, EmptyRequestDestinationOption::kUseTheEmptyString));
  }
}

TEST(RequestDestinationTest, FromStringUseFiveCharEmptyString) {
  struct {
    std::string_view input;
    std::optional<mojom::RequestDestination> expected_destination;
  } kTestCases[] = {
      {"empty", mojom::RequestDestination::kEmpty},
      {"", std::nullopt},
      {"audio", mojom::RequestDestination::kAudio},
      {"audioworklet", mojom::RequestDestination::kAudioWorklet},
      {"document", mojom::RequestDestination::kDocument},
      {"embed", mojom::RequestDestination::kEmbed},
      {"font", mojom::RequestDestination::kFont},
      {"frame", mojom::RequestDestination::kFrame},
      {"iframe", mojom::RequestDestination::kIframe},
      {"image", mojom::RequestDestination::kImage},
      {"json", mojom::RequestDestination::kJson},
      {"manifest", mojom::RequestDestination::kManifest},
      {"object", mojom::RequestDestination::kObject},
      {"paintworklet", mojom::RequestDestination::kPaintWorklet},
      {"report", mojom::RequestDestination::kReport},
      {"script", mojom::RequestDestination::kScript},
      {"serviceworker", mojom::RequestDestination::kServiceWorker},
      {"sharedworker", mojom::RequestDestination::kSharedWorker},
      {"style", mojom::RequestDestination::kStyle},
      {"track", mojom::RequestDestination::kTrack},
      {"video", mojom::RequestDestination::kVideo},
      {"webbundle", mojom::RequestDestination::kWebBundle},
      {"worker", mojom::RequestDestination::kWorker},
      {"xslt", mojom::RequestDestination::kXslt},
      {"fencedframe", mojom::RequestDestination::kFencedframe},
      {"webidentity", mojom::RequestDestination::kWebIdentity},
      {"dictionary", mojom::RequestDestination::kDictionary},
      {"speculationrules", mojom::RequestDestination::kSpeculationRules},

      {"unknown", std::nullopt},
  };
  for (const auto& testcase : kTestCases) {
    SCOPED_TRACE(testcase.input);
    EXPECT_EQ(testcase.expected_destination,
              RequestDestinationFromString(
                  testcase.input,
                  EmptyRequestDestinationOption::kUseFiveCharEmptyString));
  }
}

}  // namespace network
