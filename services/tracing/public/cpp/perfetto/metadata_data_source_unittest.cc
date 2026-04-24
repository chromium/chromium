// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

namespace tracing {

#if BUILDFLAG(IS_ANDROID)
namespace {

std::string GetAppPackageName(
    protozero::HeapBuffered<perfetto::protos::pbzero::ChromeMetadataPacket>&
        proto) {
  std::string data = proto.SerializeAsString();
  perfetto::protos::pbzero::ChromeMetadataPacket::Decoder decoder(data);
  if (decoder.has_app_package_name()) {
    return decoder.app_package_name().ToStdString();
  }
  return "";
}

}  // namespace

TEST(MetadataDataSourceTest, AndroidMetadata) {
  const std::string kTestPackage = "com.example.test";
  const std::string kPlayStore = "com.android.vending";
  const std::string kOtherStore = "com.other.store";

  // Case 1: Play Store app
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::ChromeMetadataPacket>
        proto;
    MetadataDataSource::RecordAndroidMetadata(proto.get(), /*is_system_app=*/false,
                                              kPlayStore, kTestPackage);
    EXPECT_EQ(GetAppPackageName(proto), kTestPackage);
  }

  // Case 2: System app
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::ChromeMetadataPacket>
        proto;
    MetadataDataSource::RecordAndroidMetadata(proto.get(), /*is_system_app=*/true,
                                              kOtherStore, kTestPackage);
    EXPECT_EQ(GetAppPackageName(proto), kTestPackage);
  }

  // Case 3: Other store app (not system)
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::ChromeMetadataPacket>
        proto;
    MetadataDataSource::RecordAndroidMetadata(proto.get(), /*is_system_app=*/false,
                                              kOtherStore, kTestPackage);
    EXPECT_EQ(GetAppPackageName(proto), "");
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace tracing
