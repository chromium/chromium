// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdm_remote_user_desktop_session_creator.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class GdmRemoteUserDesktopSessionCreatorTest : public testing::Test {
 protected:
  static bool IsNotSupportedError(std::string_view error) {
    return GdmRemoteUserDesktopSessionCreator::IsNotSupportedError(error);
  }
};

TEST_F(GdmRemoteUserDesktopSessionCreatorTest, UnknownMethodIsNotSupported) {
  EXPECT_TRUE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.freedesktop.DBus.Error."
      "UnknownMethod: No such method 'CreateUserDisplay'"));
}

TEST_F(GdmRemoteUserDesktopSessionCreatorTest, ServiceUnknownIsNotSupported) {
  EXPECT_TRUE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.freedesktop.DBus.Error."
      "ServiceUnknown: The name org.gnome.DisplayManager was not provided by "
      "any .service files"));
}

TEST_F(GdmRemoteUserDesktopSessionCreatorTest,
       OtherErrorsDontMeanNotSupported) {
  EXPECT_FALSE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.gtk.GDBus.UnmappedGError.Quark."
      "_gdm_2ddisplay_2derror.Code0: There's already an opened session for "
      "user alice"));
  EXPECT_FALSE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.gtk.GDBus.UnmappedGError.Quark."
      "_gdm_2ddisplay_2derror.Code0: A display for user alice has already "
      "been created"));
  EXPECT_FALSE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.freedesktop.DBus.Error.Failed: "
      "Error creating remote display"));
  EXPECT_FALSE(IsNotSupportedError(
      "Error invoking method: GDBus.Error:org.freedesktop.DBus.Error."
      "AccessDenied: Not authorized"));
  EXPECT_FALSE(IsNotSupportedError(""));
}

}  // namespace remoting
