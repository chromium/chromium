// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/supports_loading_mode/supports_loading_mode_parser.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/supports_loading_mode.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

template <typename... ModeMatchers>
::testing::Matcher<mojom::SupportsLoadingModePtr> SupportedModesAre(
    ModeMatchers&&... modes) {
  return ::testing::Pointee(::testing::Field(
      "supported_modes", &mojom::SupportsLoadingMode::supported_modes,
      ::testing::UnorderedElementsAre(modes...)));
}

TEST(SupportsLoadingModeParserTest, Valid) {
  EXPECT_THAT(ParseSupportsLoadingMode(""),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode("default"),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode("uncredentialed-prefetch"),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrefetch));
  EXPECT_THAT(ParseSupportsLoadingMode("fenced-frame"),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kFencedFrame));
  EXPECT_THAT(ParseSupportsLoadingMode(
                  "uncredentialed-prefetch, uncredentialed-prefetch"),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrefetch));
  EXPECT_THAT(ParseSupportsLoadingMode(
                  "uncredentialed-prerender, credentialed-prerender, "
                  "uncredentialed-prefetch"),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrerender,
                                mojom::LoadingMode::kCredentialedPrerender,
                                mojom::LoadingMode::kUncredentialedPrefetch));
}

TEST(SupportsLoadingModeParserTest, IgnoresUnknown) {
  EXPECT_THAT(ParseSupportsLoadingMode("quantum-entanglement"),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(
      ParseSupportsLoadingMode("quantum-entanglement, uncredentialed-prefetch"),
      SupportedModesAre(mojom::LoadingMode::kDefault,
                        mojom::LoadingMode::kUncredentialedPrefetch));
  EXPECT_THAT(ParseSupportsLoadingMode("uncredentialed-prefetch; via=mars"),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode("\"uncredentialed-prefetch\""),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode("(uncredentialed-prefetch default)"),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode("uncredentialed-pre"),
              SupportedModesAre(mojom::LoadingMode::kDefault));
}

TEST(SupportsLoadingModeParserTest, InvalidHttpStructuredHeaderList) {
  EXPECT_TRUE(ParseSupportsLoadingMode("---------------").is_null());
  EXPECT_TRUE(ParseSupportsLoadingMode(",,").is_null());
}

TEST(SupportsLoadingModeParserTest, ValidFromResponseHeaders) {
  EXPECT_TRUE(ParseSupportsLoadingMode(
                  *net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\n"))
                  .is_null());
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "Supports-Loading-Mode: \n")),
              SupportedModesAre(mojom::LoadingMode::kDefault));
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "supports-loading-mode: uncredentialed-prefetch\n")),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrefetch));
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "supports-loading-mode: uncredentialed-prefetch\n")),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrefetch));
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "supports-loading-mode: fenced-frame\n")),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kFencedFrame));
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "Supports-Loading-Mode: default,\n"
                  "  uncredentialed-prerender")),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrerender));
  EXPECT_THAT(ParseSupportsLoadingMode(*net::HttpResponseHeaders::TryToCreate(
                  "HTTP/1.1 200 OK\n"
                  "Supports-Loading-Mode: uncredentialed-prefetch\n"
                  "Content-Type: text/html; charset=utf8\n"
                  "supports-loading-mode: uncredentialed-prerender\n")),
              SupportedModesAre(mojom::LoadingMode::kDefault,
                                mojom::LoadingMode::kUncredentialedPrefetch,
                                mojom::LoadingMode::kUncredentialedPrerender));
}

}  // namespace
}  // namespace network
