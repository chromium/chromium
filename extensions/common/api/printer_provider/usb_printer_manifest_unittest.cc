// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/manifest_test.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace extensions {

class UsbPrinterManifestTest : public ManifestTest {
 public:
  UsbPrinterManifestTest() = default;
  ~UsbPrinterManifestTest() override = default;
};

TEST_F(UsbPrinterManifestTest, Filters) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("usb_printers_filters.json");
  const UsbPrinterManifestData* manifest_data =
      UsbPrinterManifestData::Get(extension.get());
  ASSERT_TRUE(manifest_data);
  ASSERT_EQ(2u, manifest_data->filters_.size());

  {
    const device::mojom::UsbDeviceFilter& filter = *manifest_data->filters_[0];
    EXPECT_TRUE(filter.vendor_id.has_value());
    EXPECT_EQ(1, filter.vendor_id.value());
    EXPECT_TRUE(filter.product_id.has_value());
    EXPECT_EQ(2, filter.product_id.value());
  }

  {
    const device::mojom::UsbDeviceFilter& filter = *manifest_data->filters_[1];
    EXPECT_TRUE(filter.vendor_id.has_value());
    EXPECT_EQ(1, filter.vendor_id.value());
    EXPECT_TRUE(filter.class_code.has_value());
    EXPECT_EQ(2, filter.class_code.value());
    EXPECT_TRUE(filter.subclass_code.has_value());
    EXPECT_EQ(3, filter.subclass_code.value());
    EXPECT_TRUE(filter.protocol_code.has_value());
    EXPECT_EQ(4, filter.protocol_code.value());
  }
}

TEST_F(UsbPrinterManifestTest, InvalidFilter) {
  LoadAndExpectError(
      "usb_printers_invalid_filter.json",
      "Only one of productId or interfaceClass may be specified.");
}

}  // namespace extensions
