// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"

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

TEST(MetadataDataSourceTest, TraceCaptureDatetimeBundleFormatting) {
  protozero::HeapBuffered<perfetto::protos::pbzero::ChromeEventBundle> bundle;

  // Test time: 2016-08-18 22:28:10 UTC
  base::Time::Exploded exploded = {
      .year = 2016,
      .month = 8,
      .day_of_month = 18,
      .hour = 22,
      .minute = 28,
      .second = 10,
  };
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCExploded(exploded, &time));

  MetadataDataSource::RecordTraceCaptureDatetime(time, bundle.get());

  std::string serialized = bundle.SerializeAsString();
  perfetto::protos::pbzero::ChromeEventBundle::Decoder decoder(serialized);

  bool found = false;
  for (auto it = decoder.metadata(); it; ++it) {
    perfetto::protos::pbzero::ChromeMetadata::Decoder metadata(*it);
    if (metadata.name().ToStdString() == "trace-capture-datetime") {
      found = true;
      EXPECT_EQ(metadata.string_value().ToStdString(), "2016-8-18 22:28:10");
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace tracing
