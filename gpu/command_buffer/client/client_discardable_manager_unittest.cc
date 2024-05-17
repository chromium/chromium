// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_discardable_manager.h"

#include "gpu/command_buffer/client/client_discardable_texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {
class FakeCommandBuffer : public CommandBuffer {
 public:
  FakeCommandBuffer() = default;
  ~FakeCommandBuffer() override { EXPECT_TRUE(active_ids_.empty()); }
  // Overridden from CommandBuffer:
  State GetLastState() override {
    NOTREACHED_IN_MIGRATION();
    return State();
  }
  void Flush(int32_t put_offset) override { NOTREACHED_IN_MIGRATION(); }
  void OrderingBarrier(int32_t put_offset) override {
    NOTREACHED_IN_MIGRATION();
  }
  State WaitForTokenInRange(int32_t start, int32_t end) override {
    NOTREACHED_IN_MIGRATION();

    return State();
  }
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override {
    NOTREACHED_IN_MIGRATION();
    return State();
  }
  void SetGetBuffer(int32_t transfer_buffer_id) override {
    NOTREACHED_IN_MIGRATION();
  }
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override {
    *id = next_id_++;
    active_ids_.insert(*id);
    return MakeMemoryBuffer(size, alignment);
  }
  void DestroyTransferBuffer(int32_t id) override {
    size_t erased_elements = active_ids_.erase(id);
    EXPECT_TRUE(erased_elements > 0);
  }
  void ForceLostContext(error::ContextLostReason reason) override {
    // No-op; doesn't need to be exercised here.
  }

 private:
  int32_t next_id_ = 1;
  std::set<int32_t> active_ids_;
};

void UnlockClientHandleForTesting(
    const ClientDiscardableHandle& client_handle) {
  ServiceDiscardableHandle service_handle(client_handle.BufferForTesting(),
                                          client_handle.byte_offset(),
                                          client_handle.shm_id());
  service_handle.Unlock();
}

bool DeleteClientHandleForTesting(
    const ClientDiscardableHandle& client_handle) {
  ServiceDiscardableHandle service_handle(client_handle.BufferForTesting(),
                                          client_handle.byte_offset(),
                                          client_handle.shm_id());
  return service_handle.Delete();
}

void UnlockAndDeleteClientHandleForTesting(
    const ClientDiscardableHandle& client_handle) {
  UnlockClientHandleForTesting(client_handle);
  EXPECT_TRUE(DeleteClientHandleForTesting(client_handle));
}

}  // namespace

TEST(ClientDiscardableManagerTest, BasicUsage) {
  FakeCommandBuffer command_buffer;
  ClientDiscardableManager manager;
  ClientDiscardableHandle::Id handle_id = manager.CreateHandle(&command_buffer);
  ClientDiscardableHandle handle = manager.GetHandle(handle_id);
  EXPECT_TRUE(handle.IsLockedForTesting());
  EXPECT_EQ(handle.shm_id(), 1);
  EXPECT_FALSE(DeleteClientHandleForTesting(handle));
  UnlockClientHandleForTesting(handle);
  manager.LockHandle(handle_id);
  EXPECT_FALSE(DeleteClientHandleForTesting(handle));
  UnlockAndDeleteClientHandleForTesting(handle);
  manager.FreeHandle(handle_id);
  manager.CheckPendingForTesting(&command_buffer);
}

TEST(ClientDiscardableManagerTest, Reuse) {
  FakeCommandBuffer command_buffer;
  ClientDiscardableManager manager;
  manager.SetElementCountForTesting(1024);
  std::vector<ClientDiscardableHandle::Id> handle_ids;
  for (int i = 0; i < 1024; ++i) {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    UnlockClientHandleForTesting(handle);
    handle_ids.push_back(handle_id);
  }
  // Delete every other entry.
  for (auto it = handle_ids.begin(); it != handle_ids.end();) {
    DeleteClientHandleForTesting(manager.GetHandle(*it));
    manager.FreeHandle(*it);
    it = handle_ids.erase(it);
    ++it;
  }
  // Allocate 512 more entries, ensure we re-use the original buffer.
  for (int i = 0; i < 512; ++i) {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    UnlockClientHandleForTesting(handle);
    handle_ids.push_back(handle_id);
  }
  // Delete all outstanding allocations
  for (const auto& handle_id : handle_ids) {
    DeleteClientHandleForTesting(manager.GetHandle(handle_id));
    manager.FreeHandle(handle_id);
  }
  manager.CheckPendingForTesting(&command_buffer);
}

TEST(ClientDiscardableManagerTest, MultipleAllocations) {
  FakeCommandBuffer command_buffer;
  ClientDiscardableManager manager;
  manager.SetElementCountForTesting(1024);
  std::vector<ClientDiscardableHandle::Id> handle_ids;
  for (int i = 1; i <= 1024; ++i) {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    UnlockClientHandleForTesting(handle);
    handle_ids.push_back(handle_id);
  }
  // Allocate and free one entry multiple times, this should cause the
  // allocation and release of a new shm_id each time.
  for (int i = 1; i < 10; ++i) {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), i + 1);
    UnlockAndDeleteClientHandleForTesting(handle);
    manager.FreeHandle(handle_id);
  }
  // Delete all outstanding allocations
  for (const auto& handle_id : handle_ids) {
    DeleteClientHandleForTesting(manager.GetHandle(handle_id));
    manager.FreeHandle(handle_id);
  }
  manager.CheckPendingForTesting(&command_buffer);
}

TEST(ClientDiscardableManagerTest, FreeDeleted) {
  FakeCommandBuffer command_buffer;
  ClientDiscardableManager manager;
  manager.SetElementCountForTesting(4);

  // Track seen IDs, we should never see an ID again, even when re-using a
  // handle.
  std::set<ClientDiscardableHandle::Id> seen_ids;

  // Fill our allocation with unlocked handles.
  std::vector<ClientDiscardableHandle::Id> handle_ids;
  for (int i = 0; i < 4; ++i) {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    EXPECT_EQ(0u, seen_ids.count(handle_id));
    seen_ids.insert(handle_id);

    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    UnlockClientHandleForTesting(handle);
    handle_ids.push_back(handle_id);
  }
  // Allocate and free a new entry. It should get a new allocation.
  {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    EXPECT_EQ(0u, seen_ids.count(handle_id));
    seen_ids.insert(handle_id);

    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 2);
    UnlockAndDeleteClientHandleForTesting(handle);
    manager.FreeHandle(handle_id);
  }
  // Delete (but don't free) one of the above entries.
  DeleteClientHandleForTesting(manager.GetHandle(handle_ids[0]));
  // Allocate and free a new entry, it should re-use the first allocation.
  {
    ClientDiscardableHandle::Id handle_id =
        manager.CreateHandle(&command_buffer);
    EXPECT_EQ(0u, seen_ids.count(handle_id));
    seen_ids.insert(handle_id);

    ClientDiscardableHandle handle = manager.GetHandle(handle_id);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    UnlockAndDeleteClientHandleForTesting(handle);
    manager.FreeHandle(handle_id);
  }
  // Delete and free the remaining handles.
  for (int i = 1; i < 4; ++i) {
    DeleteClientHandleForTesting(manager.GetHandle(handle_ids[i]));
    manager.FreeHandle(handle_ids[i]);
  }
  manager.CheckPendingForTesting(&command_buffer);
}

TEST(ClientDiscardableTextureManagerTest, BasicUsage) {
  FakeCommandBuffer command_buffer;
  ClientDiscardableTextureManager manager;
  {
    ClientDiscardableHandle handle =
        manager.InitializeTexture(&command_buffer, 1);
    EXPECT_TRUE(handle.IsLockedForTesting());
    EXPECT_EQ(handle.shm_id(), 1);
    EXPECT_FALSE(DeleteClientHandleForTesting(handle));
    UnlockClientHandleForTesting(handle);
    manager.LockTexture(1);
    EXPECT_FALSE(DeleteClientHandleForTesting(handle));
    UnlockAndDeleteClientHandleForTesting(handle);
  }
  manager.FreeTexture(1);
  manager.DiscardableManagerForTesting()->CheckPendingForTesting(
      &command_buffer);
}

}  // namespace gpu
