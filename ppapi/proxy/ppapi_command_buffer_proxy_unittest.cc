// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include "ipc/ipc_test_sink.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {
class PpapiCommandBufferProxyTest : public testing::Test,
                                    public proxy::LockedSender {
 public:
  PpapiCommandBufferProxyTest()
      : proxy_(HostResource(),
               &flush_info_,
               this,
               gpu::Capabilities(),
               gpu::GLCapabilities(),
               proxy::SerializedHandle(
                   proxy::SerializedHandle::SHARED_MEMORY_REGION),
               gpu::CommandBufferId()) {}

  ~PpapiCommandBufferProxyTest() override {}

 protected:
  // We can't verify sync message behavior with this setup.
  bool SendAndStayLocked(IPC::Message* msg) override { return sink_.Send(msg); }

  IPC::TestSink sink_;
  proxy::InstanceData::FlushInfo flush_info_;
  proxy::PpapiCommandBufferProxy proxy_;
};

TEST_F(PpapiCommandBufferProxyTest, OrderingBarriersAreCoalescedWithFlush) {
  proxy_.OrderingBarrier(10);
  proxy_.OrderingBarrier(20);
  proxy_.OrderingBarrier(30);
  proxy_.Flush(40);

  EXPECT_EQ(1u, sink_.message_count());
  const IPC::Message* msg =
      sink_.GetFirstMessageMatching(PpapiHostMsg_PPBGraphics3D_AsyncFlush::ID);
  ASSERT_TRUE(msg);
  PpapiHostMsg_PPBGraphics3D_AsyncFlush::Param params;
  ASSERT_TRUE(PpapiHostMsg_PPBGraphics3D_AsyncFlush::Read(msg, &params));
  int32_t sent_put_offset = std::get<1>(params);
  EXPECT_EQ(40, sent_put_offset);
  EXPECT_FALSE(flush_info_.flush_pending);
  EXPECT_EQ(40, flush_info_.put_offset);
}

TEST_F(PpapiCommandBufferProxyTest, FlushPendingWorkFlushesOrderingBarriers) {
  proxy_.OrderingBarrier(10);
  proxy_.OrderingBarrier(20);
  proxy_.OrderingBarrier(30);
  proxy_.FlushPendingWork();

  EXPECT_EQ(1u, sink_.message_count());
  const IPC::Message* msg =
      sink_.GetFirstMessageMatching(PpapiHostMsg_PPBGraphics3D_AsyncFlush::ID);
  ASSERT_TRUE(msg);
  PpapiHostMsg_PPBGraphics3D_AsyncFlush::Param params;
  ASSERT_TRUE(PpapiHostMsg_PPBGraphics3D_AsyncFlush::Read(msg, &params));
  int32_t sent_put_offset = std::get<1>(params);
  EXPECT_EQ(30, sent_put_offset);
  EXPECT_FALSE(flush_info_.flush_pending);
  EXPECT_EQ(30, flush_info_.put_offset);
}

TEST_F(PpapiCommandBufferProxyTest, EnsureWorkVisibleFlushesOrderingBarriers) {
  proxy_.OrderingBarrier(10);
  proxy_.OrderingBarrier(20);
  proxy_.OrderingBarrier(30);
  proxy_.EnsureWorkVisible();

  EXPECT_EQ(2u, sink_.message_count());
  const IPC::Message* msg = sink_.GetMessageAt(0);
  ASSERT_TRUE(msg);
  EXPECT_EQ(static_cast<uint32_t>(PpapiHostMsg_PPBGraphics3D_AsyncFlush::ID),
            msg->type());
  PpapiHostMsg_PPBGraphics3D_AsyncFlush::Param params;
  ASSERT_TRUE(PpapiHostMsg_PPBGraphics3D_AsyncFlush::Read(msg, &params));
  int32_t sent_put_offset = std::get<1>(params);
  EXPECT_EQ(30, sent_put_offset);
  EXPECT_FALSE(flush_info_.flush_pending);
  EXPECT_EQ(30, flush_info_.put_offset);

  msg = sink_.GetMessageAt(1);
  ASSERT_TRUE(msg);
  EXPECT_EQ(
      static_cast<uint32_t>(PpapiHostMsg_PPBGraphics3D_EnsureWorkVisible::ID),
      msg->type());
}

}  // namespace ppapi
