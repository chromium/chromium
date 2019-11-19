// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gles2_query_manager.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gpu_timing.h"
#include "ui/gl/gpu_timing_fake.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

class QueryManagerTest : public GpuServiceTest {
 public:
  static const uint32_t kSharedMemoryOffset = 132;
  static const uint32_t kSharedMemory2Offset = 232;
  static const size_t kSharedBufferSize = 2048;
  static const int32_t kInvalidSharedMemoryId = -1;
  static const uint32_t kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32_t kInitialResult = 0xBDBDBDBDu;
  static const uint8_t kInitialMemoryValue = 0xBDu;

  QueryManagerTest() = default;
  ~QueryManagerTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUpWithGLVersion("3.2",
                                       "GL_ARB_occlusion_query, "
                                       "GL_ARB_timer_query");
    SetUpMockGL("GL_EXT_occlusion_query_boolean, GL_ARB_timer_query");
  }

  void TearDown() override {
    decoder_.reset();
    manager_->Destroy(false);
    manager_.reset();
    command_buffer_service_.reset();
    GpuServiceTest::TearDown();
  }

  void SetUpMockGL(const char* extension_expectations) {
    command_buffer_service_.reset(new FakeCommandBufferServiceBase());
    scoped_refptr<gpu::Buffer> buffer =
        command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                            &shared_memory_id_);
    memset(buffer->memory(), kInitialMemoryValue, kSharedBufferSize);
    buffer = command_buffer_service_->CreateTransferBufferHelper(
        kSharedBufferSize, &shared_memory2_id_);
    memset(buffer->memory(), kInitialMemoryValue, kSharedBufferSize);
    decoder_.reset(new MockGLES2Decoder(&client_, command_buffer_service_.get(),
                                        &outputter_));
    TestHelper::SetupFeatureInfoInitExpectations(
        gl_.get(), extension_expectations);
    EXPECT_CALL(*decoder_.get(), GetGLContext())
      .WillRepeatedly(Return(GetGLContext()));
    scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
    feature_info->InitializeForTesting();
    manager_.reset(new GLES2QueryManager(decoder_.get(), feature_info.get()));
  }

  QueryManager::Query* CreateQuery(GLenum target,
                                   GLuint client_id,
                                   int32_t shm_id,
                                   uint32_t shm_offset,
                                   GLuint service_id) {
    return CreateQueryOnManager(manager_.get(), target, client_id, shm_id,
                                shm_offset, service_id);
  }

  QueryManager::Query* CreateQueryOnManager(QueryManager* manager,
                                            GLenum target,
                                            GLuint client_id,
                                            int32_t shm_id,
                                            uint32_t shm_offset,
                                            GLuint service_id) {
    if (service_id) {
      EXPECT_CALL(*gl_, GenQueries(1, _))
          .WillOnce(SetArgPointee<1>(service_id))
          .RetiresOnSaturation();
    }
    scoped_refptr<gpu::Buffer> buffer = decoder_->GetSharedMemoryBuffer(shm_id);
    if (!buffer)
      return nullptr;
    QuerySync* sync = static_cast<QuerySync*>(
        buffer->GetDataAddress(shm_offset, sizeof(QuerySync)));
    if (!sync)
      return nullptr;
    return manager->CreateQuery(target, client_id, std::move(buffer), sync);
  }

  void QueueQuery(QueryManager::Query* query,
                  GLuint service_id,
                  base::subtle::Atomic32 submit_count) {
    EXPECT_CALL(*gl_, BeginQuery(query->target(), service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, EndQuery(query->target()))
        .Times(1)
        .RetiresOnSaturation();
    manager_->BeginQuery(query);
    manager_->EndQuery(query, submit_count);
  }

  TraceOutputter outputter_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  FakeDecoderClient client_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
  std::unique_ptr<GLES2QueryManager> manager_;

  int32_t shared_memory_id_ = 0;
  int32_t shared_memory2_id_ = 0;
};

class QueryManagerManualSetupTest : public QueryManagerTest {
 protected:
  void SetUp() override {
    // Let test setup manually.
  }
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const uint32_t QueryManagerTest::kSharedMemoryOffset;
const uint32_t QueryManagerTest::kSharedMemory2Offset;
const size_t QueryManagerTest::kSharedBufferSize;
const int32_t QueryManagerTest::kInvalidSharedMemoryId;
const uint32_t QueryManagerTest::kInvalidSharedMemoryOffset;
const uint32_t QueryManagerTest::kInitialResult;
const uint8_t QueryManagerTest::kInitialMemoryValue;
#endif

TEST_F(QueryManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;

  EXPECT_FALSE(manager_->HavePendingQueries());
  // Check we can create a Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(GL_ANY_SAMPLES_PASSED_EXT, kClient1Id, shared_memory_id_,
                  kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != nullptr);
  // Check we can get the same Query.
  EXPECT_EQ(query.get(), manager_->GetQuery(kClient1Id));
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient2Id) == nullptr);
  // Check we can delete the query.
  EXPECT_CALL(*gl_, DeleteQueries(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->RemoveQuery(kClient1Id);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == nullptr);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
  EXPECT_FALSE(manager_->HavePendingQueries());
}

TEST_F(QueryManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(GL_ANY_SAMPLES_PASSED_EXT, kClient1Id, shared_memory_id_,
                  kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != nullptr);
  EXPECT_CALL(*gl_, DeleteQueries(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy(true);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == nullptr);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
}

TEST_F(QueryManagerTest, QueryBasic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset,
                  kService1Id));
  ASSERT_TRUE(query.get() != nullptr);

  EXPECT_TRUE(query->IsValid());
  EXPECT_FALSE(query->IsDeleted());
  EXPECT_FALSE(query->IsPending());
  EXPECT_EQ(kTarget, query->target());
}

TEST_F(QueryManagerTest, ProcessPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;
  const GLuint kResult = 1;

  // Check nothing happens if there are no pending queries.
  manager_->ProcessPendingQueries(false);

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset,
                  kService1Id));
  ASSERT_TRUE(query.get() != nullptr);

  // Setup shared memory like client would.
  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  ASSERT_TRUE(sync != nullptr);
  sync->Reset();

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);
  EXPECT_TRUE(query->IsPending());
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with return not available.
  // Expect 1 GL command.
  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  manager_->ProcessPendingQueries(false);
  EXPECT_TRUE(query->IsPending());
  EXPECT_EQ(0, sync->process_count);
  EXPECT_EQ(0u, sync->result);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgPointee<2>(kResult))
      .RetiresOnSaturation();
  manager_->ProcessPendingQueries(false);
  EXPECT_FALSE(query->IsPending());
  EXPECT_EQ(kSubmitCount, sync->process_count);
  EXPECT_EQ(kResult, sync->result);
  EXPECT_FALSE(manager_->HavePendingQueries());

  // Process with no queries.
  // Expect no GL commands/
  manager_->ProcessPendingQueries(false);
}

TEST_F(QueryManagerTest, ProcessPendingQueries) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  const GLuint kService2Id = 12;
  const GLuint kClient3Id = 3;
  const GLuint kService3Id = 13;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const base::subtle::Atomic32 kSubmitCount1 = 123;
  const base::subtle::Atomic32 kSubmitCount2 = 123;
  const base::subtle::Atomic32 kSubmitCount3 = 123;
  const GLuint kResult1 = 1;
  const GLuint kResult2 = 1;
  const GLuint kResult3 = 1;

  // Setup shared memory like client would.
  QuerySync* sync1 = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync1) * 3);
  ASSERT_TRUE(sync1 != nullptr);
  QuerySync* sync2 = sync1 + 1;
  QuerySync* sync3 = sync2 + 1;

  // Create Queries.
  scoped_refptr<QueryManager::Query> query1(
      CreateQuery(kTarget, kClient1Id, shared_memory_id_,
                  kSharedMemoryOffset + sizeof(*sync1) * 0, kService1Id));
  scoped_refptr<QueryManager::Query> query2(
      CreateQuery(kTarget, kClient2Id, shared_memory_id_,
                  kSharedMemoryOffset + sizeof(*sync1) * 1, kService2Id));
  scoped_refptr<QueryManager::Query> query3(
      CreateQuery(kTarget, kClient3Id, shared_memory_id_,
                  kSharedMemoryOffset + sizeof(*sync1) * 2, kService3Id));
  ASSERT_TRUE(query1.get() != nullptr);
  ASSERT_TRUE(query2.get() != nullptr);
  ASSERT_TRUE(query3.get() != nullptr);
  EXPECT_FALSE(manager_->HavePendingQueries());

  sync1->Reset();
  sync2->Reset();
  sync3->Reset();

  // Queue them
  QueueQuery(query1.get(), kService1Id, kSubmitCount1);
  QueueQuery(query2.get(), kService2Id, kSubmitCount2);
  QueueQuery(query3.get(), kService3Id, kSubmitCount3);
  EXPECT_TRUE(query1->IsPending());
  EXPECT_TRUE(query2->IsPending());
  EXPECT_TRUE(query3->IsPending());
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with return available for first 2 queries.
  // Expect 4 GL commands.
  {
    InSequence s;
    EXPECT_CALL(
        *gl_, GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgPointee<2>(kResult1))
        .RetiresOnSaturation();
    EXPECT_CALL(
        *gl_, GetQueryObjectuiv(kService2Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetQueryObjectuiv(kService2Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgPointee<2>(kResult2))
        .RetiresOnSaturation();
    EXPECT_CALL(
        *gl_, GetQueryObjectuiv(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgPointee<2>(0))
        .RetiresOnSaturation();
    manager_->ProcessPendingQueries(false);
  }
  EXPECT_FALSE(query1->IsPending());
  EXPECT_FALSE(query2->IsPending());
  EXPECT_TRUE(query3->IsPending());
  EXPECT_EQ(kSubmitCount1, sync1->process_count);
  EXPECT_EQ(kSubmitCount2, sync2->process_count);
  EXPECT_EQ(kResult1, sync1->result);
  EXPECT_EQ(kResult2, sync2->result);
  EXPECT_EQ(0, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with renaming query. No result.
  // Expect 1 GL commands.
  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  manager_->ProcessPendingQueries(false);
  EXPECT_TRUE(query3->IsPending());
  EXPECT_EQ(0, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with renaming query. With result.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryObjectuiv(kService3Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgPointee<2>(kResult3))
      .RetiresOnSaturation();
  manager_->ProcessPendingQueries(false);
  EXPECT_FALSE(query3->IsPending());
  EXPECT_EQ(kSubmitCount3, sync3->process_count);
  EXPECT_EQ(kResult3, sync3->result);
  EXPECT_FALSE(manager_->HavePendingQueries());
}

TEST_F(QueryManagerTest, ExitWithPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset,
                  kService1Id));
  ASSERT_TRUE(query.get() != nullptr);

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);
}

// Test that when based on ARB_occlusion_query2 we use GL_ANY_SAMPLES_PASSED_ARB
// for GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT
TEST_F(QueryManagerTest, ARBOcclusionQuery2) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query2");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  std::unique_ptr<GLES2QueryManager> manager(
      new GLES2QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query =
      CreateQueryOnManager(manager.get(), kTarget, kClient1Id,
                           shared_memory_id_, kSharedMemoryOffset, kService1Id);
  ASSERT_TRUE(query != nullptr);

  EXPECT_CALL(*gl_, BeginQuery(GL_ANY_SAMPLES_PASSED_EXT, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQuery(GL_ANY_SAMPLES_PASSED_EXT))
      .Times(1)
      .RetiresOnSaturation();
  manager->BeginQuery(query);
  manager->EndQuery(query, kSubmitCount);
  manager->Destroy(false);
}

// Test that when based on ARB_occlusion_query we use GL_SAMPLES_PASSED_ARB
// for GL_ANY_SAMPLES_PASSED_EXT
TEST_F(QueryManagerTest, ARBOcclusionQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  std::unique_ptr<GLES2QueryManager> manager(
      new GLES2QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query =
      CreateQueryOnManager(manager.get(), kTarget, kClient1Id,
                           shared_memory_id_, kSharedMemoryOffset, kService1Id);
  ASSERT_TRUE(query != nullptr);

  EXPECT_CALL(*gl_, BeginQuery(GL_SAMPLES_PASSED_ARB, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQuery(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  manager->BeginQuery(query);
  manager->EndQuery(query, kSubmitCount);
  manager->Destroy(false);
}

TEST_F(QueryManagerTest, ARBOcclusionPauseResume) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  std::unique_ptr<GLES2QueryManager> manager(
      new GLES2QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query =
      CreateQueryOnManager(manager.get(), kTarget, kClient1Id,
                           shared_memory_id_, kSharedMemoryOffset, kService1Id);
  ASSERT_TRUE(query != nullptr);

  EXPECT_CALL(*gl_, BeginQuery(GL_SAMPLES_PASSED_ARB, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager->BeginQuery(query);

  // Pause and Resume the manager.
  EXPECT_CALL(*gl_, EndQuery(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  manager->PauseQueries();

  EXPECT_CALL(*gl_, GenQueries(1, _))
      .WillOnce(SetArgPointee<1>(kService2Id))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BeginQuery(GL_SAMPLES_PASSED_ARB, kService2Id))
      .Times(1)
      .RetiresOnSaturation();
  manager->ResumeQueries();

  EXPECT_CALL(*gl_, EndQuery(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  manager->EndQuery(query, kSubmitCount);

  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService2Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(1u))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgPointee<2>(0u))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryObjectuiv(kService2Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgPointee<2>(1u))
      .RetiresOnSaturation();
  manager->ProcessPendingQueries(false);
  EXPECT_TRUE(query->IsFinished());

  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  EXPECT_EQ(1u, sync->result);

  // Make sure new query still works.
  EXPECT_CALL(*gl_, DeleteQueries(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BeginQuery(GL_SAMPLES_PASSED_ARB, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQuery(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  manager->BeginQuery(query);
  manager->EndQuery(query, kSubmitCount + 1);

  EXPECT_CALL(*gl_,
              GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgPointee<2>(1u))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryObjectuiv(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgPointee<2>(0u))
      .RetiresOnSaturation();
  manager->ProcessPendingQueries(false);
  EXPECT_TRUE(query->IsFinished());

  EXPECT_EQ(0u, sync->result);
  EXPECT_CALL(*gl_, DeleteQueries(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager->Destroy(true);
}

TEST_F(QueryManagerTest, TimeElapsedQuery) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIME_ELAPSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;
  gl::GPUTimingFake fake_timing_queries;
  decoder_->GetGLContext()->CreateGPUTimingClient()->SetCpuTimeForTesting(
      base::BindRepeating(&gl::GPUTimingFake::GetFakeCPUTime));

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  fake_timing_queries.ExpectGPUTimerQuery(*gl_, true);
  fake_timing_queries.SetCurrentGLTime(
      200 * base::Time::kNanosecondsPerMicrosecond);
  manager_->BeginQuery(query);
  fake_timing_queries.SetCurrentGLTime(
      300 * base::Time::kNanosecondsPerMicrosecond);
  manager_->EndQuery(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());

  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  const uint64_t expected_result =
      100u * base::Time::kNanosecondsPerMicrosecond;
  EXPECT_EQ(expected_result, sync->result);

  manager_->Destroy(false);
}

TEST_F(QueryManagerTest, TimeElapsedPauseResume) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIME_ELAPSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;
  gl::GPUTimingFake fake_timing_queries;
  decoder_->GetGLContext()->CreateGPUTimingClient()->SetCpuTimeForTesting(
      base::BindRepeating(&gl::GPUTimingFake::GetFakeCPUTime));

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  fake_timing_queries.ExpectGPUTimerQuery(*gl_, true);
  fake_timing_queries.SetCurrentGLTime(
      200 * base::Time::kNanosecondsPerMicrosecond);
  manager_->BeginQuery(query);

  // Pause and Resume here.
  fake_timing_queries.SetCurrentGLTime(
      300 * base::Time::kNanosecondsPerMicrosecond);
  manager_->PauseQueries();

  fake_timing_queries.SetCurrentGLTime(
      400 * base::Time::kNanosecondsPerMicrosecond);
  manager_->ResumeQueries();

  fake_timing_queries.SetCurrentGLTime(
      500 * base::Time::kNanosecondsPerMicrosecond);
  manager_->EndQuery(query, kSubmitCount);

  manager_->ProcessPendingQueries(false);
  EXPECT_TRUE(query->IsFinished());

  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  const uint64_t expected_result =
      300u * base::Time::kNanosecondsPerMicrosecond;
  EXPECT_EQ(expected_result, sync->result);

  // Make sure next query works properly.
  fake_timing_queries.SetCurrentGLTime(
      600 * base::Time::kNanosecondsPerMicrosecond);
  manager_->BeginQuery(query);
  fake_timing_queries.SetCurrentGLTime(
      700 * base::Time::kNanosecondsPerMicrosecond);
  manager_->EndQuery(query, kSubmitCount + 1);
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());

  const uint64_t expected_result2 =
      100u * base::Time::kNanosecondsPerMicrosecond;
  EXPECT_EQ(expected_result2, sync->result);

  manager_->Destroy(false);
}

TEST_F(QueryManagerManualSetupTest, TimeElapsedDisjoint) {
  GpuServiceTest::SetUpWithGLVersion("OpenGL ES 3.0",
                                     "GL_EXT_disjoint_timer_query");
  gl::GPUTimingFake fake_timing_queries;
  fake_timing_queries.ExpectDisjointCalls(*gl_);
  SetUpMockGL("GL_EXT_disjoint_timer_query");

  DisjointValueSync* disjoint_sync =
      decoder_->GetSharedMemoryAs<DisjointValueSync*>(
          shared_memory2_id_, kSharedMemory2Offset, sizeof(*disjoint_sync));
  manager_->SetDisjointSync(shared_memory2_id_, kSharedMemory2Offset);

  const uint32_t current_disjoint_value = disjoint_sync->GetDisjointCount();
  ASSERT_EQ(0u, current_disjoint_value);

  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIME_ELAPSED_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  // Disjoint happening before the query should not trigger a disjoint event.
  fake_timing_queries.SetDisjoint();

  fake_timing_queries.ExpectGPUTimerQuery(*gl_, true);
  manager_->BeginQuery(query);
  manager_->EndQuery(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());
  EXPECT_EQ(current_disjoint_value, disjoint_sync->GetDisjointCount());

  // Disjoint happening during query should trigger disjoint event.
  fake_timing_queries.ExpectGPUTimerQuery(*gl_, true);
  manager_->BeginQuery(query);
  fake_timing_queries.SetDisjoint();
  manager_->EndQuery(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());
  EXPECT_NE(current_disjoint_value, disjoint_sync->GetDisjointCount());

  manager_->Destroy(false);
}

TEST_F(QueryManagerTest, TimeStampQuery) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIMESTAMP_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;
  gl::GPUTimingFake fake_timing_queries;

  decoder_->GetGLContext()->CreateGPUTimingClient()->SetCpuTimeForTesting(
      base::BindRepeating(&gl::GPUTimingFake::GetFakeCPUTime));

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  const uint64_t expected_result =
      100u * base::Time::kNanosecondsPerMicrosecond;
  fake_timing_queries.SetCurrentGLTime(expected_result);
  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  EXPECT_EQ(expected_result, sync->result);

  manager_->Destroy(false);
}

TEST_F(QueryManagerTest, TimeStampQueryPending) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIMESTAMP_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;
  gl::GPUTimingFake fake_timing_queries;

  decoder_->GetGLContext()->CreateGPUTimingClient()->SetCpuTimeForTesting(
      base::BindRepeating(&gl::GPUTimingFake::GetFakeCPUTime));

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  const uint64_t expected_result =
      100u * base::Time::kNanosecondsPerMicrosecond;
  fake_timing_queries.SetCurrentGLTime(expected_result);
  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  EXPECT_TRUE(query->IsPending());
  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  EXPECT_EQ(expected_result, sync->result);

  manager_->Destroy(false);
}

TEST_F(QueryManagerManualSetupTest, TimeStampDisjoint) {
  GpuServiceTest::SetUpWithGLVersion("OpenGL ES 3.0",
                                     "GL_EXT_disjoint_timer_query");
  gl::GPUTimingFake fake_timing_queries;
  fake_timing_queries.ExpectDisjointCalls(*gl_);
  SetUpMockGL("GL_EXT_disjoint_timer_query");

  DisjointValueSync* disjoint_sync =
      decoder_->GetSharedMemoryAs<DisjointValueSync*>(
          shared_memory2_id_, kSharedMemory2Offset, sizeof(*disjoint_sync));
  manager_->SetDisjointSync(shared_memory2_id_, kSharedMemory2Offset);

  const uint32_t current_disjoint_value = disjoint_sync->GetDisjointCount();
  ASSERT_EQ(0u, current_disjoint_value);

  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIMESTAMP_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  // Disjoint happening before the query should not trigger a disjoint event.
  fake_timing_queries.SetDisjoint();

  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());
  EXPECT_EQ(current_disjoint_value, disjoint_sync->GetDisjointCount());

  // Disjoint happening during query should trigger disjoint event.
  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  fake_timing_queries.SetDisjoint();
  manager_->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsFinished());
  EXPECT_NE(current_disjoint_value, disjoint_sync->GetDisjointCount());

  manager_->Destroy(false);
}

TEST_F(QueryManagerManualSetupTest, DisjointContinualTest) {
  GpuServiceTest::SetUpWithGLVersion("OpenGL ES 3.0",
                                     "GL_EXT_disjoint_timer_query");
  gl::GPUTimingFake fake_timing_queries;
  fake_timing_queries.ExpectDisjointCalls(*gl_);
  SetUpMockGL("GL_EXT_disjoint_timer_query");

  DisjointValueSync* disjoint_sync =
      decoder_->GetSharedMemoryAs<DisjointValueSync*>(
          shared_memory2_id_, kSharedMemory2Offset, sizeof(*disjoint_sync));
  manager_->SetDisjointSync(shared_memory2_id_, kSharedMemory2Offset);

  const uint32_t current_disjoint_value = disjoint_sync->GetDisjointCount();
  ASSERT_EQ(0u, current_disjoint_value);

  // Disjoint value should not be updated until we have a timestamp query.
  fake_timing_queries.SetDisjoint();
  manager_->ProcessFrameBeginUpdates();
  EXPECT_EQ(current_disjoint_value, disjoint_sync->GetDisjointCount());

  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_TIMESTAMP_EXT;
  const base::subtle::Atomic32 kSubmitCount = 123;

  QueryManager::Query* query = CreateQuery(
      kTarget, kClient1Id, shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  fake_timing_queries.ExpectGPUTimeStampQuery(*gl_, false);
  manager_->QueryCounter(query, kSubmitCount);
  manager_->ProcessPendingQueries(false);

  EXPECT_EQ(current_disjoint_value, disjoint_sync->GetDisjointCount());
  fake_timing_queries.SetDisjoint();
  manager_->ProcessFrameBeginUpdates();
  EXPECT_NE(current_disjoint_value, disjoint_sync->GetDisjointCount());

  manager_->Destroy(false);
}

TEST_F(QueryManagerTest, GetErrorQuery) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_GET_ERROR_QUERY_CHROMIUM;
  const base::subtle::Atomic32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(gl_.get(), "");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  std::unique_ptr<GLES2QueryManager> manager(
      new GLES2QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query =
      CreateQueryOnManager(manager.get(), kTarget, kClient1Id,
                           shared_memory_id_, kSharedMemoryOffset, 0);
  ASSERT_TRUE(query != nullptr);

  // Setup shared memory like client would.
  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      shared_memory_id_, kSharedMemoryOffset, sizeof(*sync));
  ASSERT_TRUE(sync != nullptr);
  sync->Reset();

  manager->BeginQuery(query);

  MockErrorState mock_error_state;
  EXPECT_CALL(*decoder_.get(), GetErrorState())
      .WillRepeatedly(Return(&mock_error_state));
  EXPECT_CALL(mock_error_state, GetGLError())
      .WillOnce(Return(GL_INVALID_ENUM))
      .RetiresOnSaturation();

  manager->EndQuery(query, kSubmitCount);
  EXPECT_FALSE(query->IsPending());

  EXPECT_EQ(static_cast<GLuint>(GL_INVALID_ENUM), sync->result);

  manager->Destroy(false);
}

TEST_F(QueryManagerTest, OcclusionQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_SAMPLES_PASSED_ARB;
  const base::subtle::Atomic32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  std::unique_ptr<GLES2QueryManager> manager(
      new GLES2QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query =
      CreateQueryOnManager(manager.get(), kTarget, kClient1Id,
                           shared_memory_id_, kSharedMemoryOffset, kService1Id);
  ASSERT_TRUE(query != nullptr);

  EXPECT_CALL(*gl_, BeginQuery(GL_SAMPLES_PASSED_ARB, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQuery(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  manager->BeginQuery(query);
  manager->EndQuery(query, kSubmitCount);
  manager->Destroy(false);
}

}  // namespace gles2
}  // namespace gpu
