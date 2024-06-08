// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/public/common/webplugininfo.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PluginUtilsTest, VersionExtraction) {
  // Some real-world plugin versions (spaces, commata, parentheses, 'r', oh my)
  const char* versions[][2] = {
    { "7.6.6 (1671)", "7.6.6.1671" },  // Quicktime
    { "2, 0, 0, 254", "2.0.0.254" },   // DivX
    { "3, 0, 0, 0", "3.0.0.0" },       // Picasa
    { "1, 0, 0, 1", "1.0.0.1" },       // Earth
    { "10,0,45,2", "10.0.45.2" },      // Flash
    { "10.1 r102", "10.1.102"},        // Flash
    { "10.3 d180", "10.3.180" },       // Flash (Debug)
    { "11.5.7r609", "11.5.7.609"},     // Shockwave
    { "1.6.0_22", "1.6.0.22"},         // Java
    { "1.07.00_0005", "1.7.0.5"},      // Java with leading zeros
    { "1..0", "1.0.0" }                // Empty version component
  };

  for (size_t i = 0; i < std::size(versions); i++) {
    base::Version version;
    WebPluginInfo::CreateVersionFromString(
        base::ASCIIToUTF16(versions[i][0]), &version);

    ASSERT_TRUE(version.IsValid());
    EXPECT_EQ(versions[i][1], version.GetString());
  }
}

}  // namespace content
