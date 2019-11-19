// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_interface_chromeos.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace api {

// Tests of networking_private_crypto support for Networking Private API.
class DocumentScanInterfaceChromeosTest : public testing::Test {
 public:
  DocumentScanInterfaceChromeosTest() = default;
  ~DocumentScanInterfaceChromeosTest() override = default;

  void SetUp() override { chromeos::DBusThreadManager::Initialize(); }

  void TearDown() override { chromeos::DBusThreadManager::Shutdown(); }

  chromeos::FakeLorgnetteManagerClient* GetLorgnetteManagerClient() {
    return static_cast<chromeos::FakeLorgnetteManagerClient*>(
        chromeos::DBusThreadManager::Get()->GetLorgnetteManagerClient());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  DocumentScanInterfaceChromeos scan_interface_;
};

TEST_F(DocumentScanInterfaceChromeosTest, ListScanners) {
  // Use constexpr const char* instead of constexpr char[] because implicit
  // conversion in lambda doesn't work.
  constexpr const char* kScannerName = "Monet";
  constexpr const char* kScannerManufacturer = "Jacques-Louis David";
  constexpr const char* kScannerModel = "Le Havre";
  constexpr const char* kScannerType = "Impressionism";
  GetLorgnetteManagerClient()->AddScannerTableEntry(
      kScannerName,
      {{lorgnette::kScannerPropertyManufacturer, kScannerManufacturer},
       {lorgnette::kScannerPropertyModel, kScannerModel},
       {lorgnette::kScannerPropertyType, kScannerType}});

  base::RunLoop run_loop;
  scan_interface_.ListScanners(base::Bind(
      [](base::RunLoop* run_loop,
         const std::vector<DocumentScanInterface::ScannerDescription>&
             descriptions,
         const std::string& error) {
        run_loop->Quit();
        ASSERT_EQ(1u, descriptions.size());
        // Wrap by std::string explicitly, because const reference of the
        // constexpr in the enclosing scope, which EXPECT_EQ macro uses,
        // cannot be taken.
        EXPECT_EQ(std::string(kScannerName), descriptions[0].name);
        EXPECT_EQ(std::string(kScannerManufacturer),
                  descriptions[0].manufacturer);
        EXPECT_EQ(std::string(kScannerModel), descriptions[0].model);
        EXPECT_EQ(std::string(kScannerType), descriptions[0].scanner_type);
        EXPECT_EQ("image/png", descriptions[0].image_mime_type);
        EXPECT_EQ("", error);
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanFailure) {
  base::RunLoop run_loop;
  scan_interface_.Scan(
      "Monet", DocumentScanInterface::kScanModeColor, 4096,
      base::Bind(
          [](base::RunLoop* run_loop, const std::string& scanned_image,
             const std::string& mime_type, const std::string& error) {
            run_loop->Quit();
            EXPECT_EQ("", scanned_image);
            EXPECT_EQ("", mime_type);
            EXPECT_EQ("Image scan failed", error);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanSuccess) {
  constexpr char kScannerName[] = "Monet";
  constexpr int kResolution = 4096;
  GetLorgnetteManagerClient()->AddScanData(
      kScannerName,
      chromeos::LorgnetteManagerClient::ScanProperties{
          lorgnette::kScanPropertyModeColor, kResolution},
      "PrettyPicture");
  base::RunLoop run_loop;
  scan_interface_.Scan(
      kScannerName, DocumentScanInterface::kScanModeColor, kResolution,
      base::Bind(
          [](base::RunLoop* run_loop, const std::string& scanned_image,
             const std::string& mime_type, const std::string& error) {
            run_loop->Quit();
            // Data URL plus base64 representation of "PrettyPicture".
            EXPECT_EQ("data:image/png;base64,UHJldHR5UGljdHVyZQ==",
                      scanned_image);
            EXPECT_EQ("image/png", mime_type);
            EXPECT_EQ("", error);
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace api

}  // namespace extensions
