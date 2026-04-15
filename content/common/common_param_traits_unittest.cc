// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/pickle.h"
#include "base/values.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/content_constants.h"
#include "ipc/param_traits_utils.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_policy_status.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

// Tests std::pair serialization
TEST(IPCMessageTest, Pair) {
  typedef std::pair<std::string, std::string> TestPair;

  TestPair input("foo", "bar");
  base::Pickle msg;
  IPC::ParamTraits<TestPair>::Write(&msg, input);

  TestPair output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<TestPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(output.first, "foo");
  EXPECT_EQ(output.second, "bar");
}

// Tests bitmap serialization.
TEST(IPCMessageTest, ValueDict) {
  base::DictValue input;
  input.Set("null", base::Value());
  input.Set("bool", true);
  input.Set("int", 42);

  base::DictValue subdict;
  subdict.Set("str", "forty two");
  subdict.Set("bool", false);

  base::ListValue sublist;
  sublist.Append(42.42);
  sublist.Append("forty");
  sublist.Append("two");
  subdict.Set("list", std::move(sublist));

  input.Set("dict", std::move(subdict));

  base::Pickle msg;
  IPC::WriteParam(&msg, input);

  base::DictValue output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_EQ(input, output);

  // Also test the corrupt case.
  base::Pickle bad_msg;
  bad_msg.WriteInt(99);
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(IPCMessageTest, SurfaceInfo) {
  base::Pickle msg;
  const viz::SurfaceId kArbitrarySurfaceId(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(3, base::UnguessableToken::Create()));
  constexpr float kArbitraryDeviceScaleFactor = 0.9f;
  const gfx::Size kArbitrarySize(65, 321);
  const viz::SurfaceInfo surface_info_in(
      kArbitrarySurfaceId, kArbitraryDeviceScaleFactor, kArbitrarySize);
  IPC::ParamTraits<viz::SurfaceInfo>::Write(&msg, surface_info_in);

  viz::SurfaceInfo surface_info_out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<viz::SurfaceInfo>::Read(&msg, &iter, &surface_info_out));

  ASSERT_EQ(surface_info_in, surface_info_out);
}
