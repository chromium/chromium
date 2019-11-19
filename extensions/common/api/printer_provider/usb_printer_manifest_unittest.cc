// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/value_builder.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace extensions {

class UsbPrinterManifestTest : public ManifestTest {
 public:
  UsbPrinterManifestTest() {}
  ~UsbPrinterManifestTest() override {}
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
    EXPECT_TRUE(filter.has_vendor_id);
    EXPECT_EQ(1, filter.vendor_id);
    EXPECT_TRUE(filter.has_product_id);
    EXPECT_EQ(2, filter.product_id);
  }

  {
    const device::mojom::UsbDeviceFilter& filter = *manifest_data->filters_[1];
    EXPECT_TRUE(filter.has_vendor_id);
    EXPECT_EQ(1, filter.vendor_id);
    EXPECT_TRUE(filter.has_class_code);
    EXPECT_EQ(2, filter.class_code);
    EXPECT_TRUE(filter.has_subclass_code);
    EXPECT_EQ(3, filter.subclass_code);
    EXPECT_TRUE(filter.has_protocol_code);
    EXPECT_EQ(4, filter.protocol_code);
  }
}

TEST_F(UsbPrinterManifestTest, InvalidFilter) {
  LoadAndExpectError(
      "usb_printers_invalid_filter.json",
      "Only one of productId or interfaceClass may be specified.");
}

}  // namespace extensions
