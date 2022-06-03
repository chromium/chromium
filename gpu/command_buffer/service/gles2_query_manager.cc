// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_query_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {
namespace gles2 {

namespace {

class AbstractIntegerQuery : public QueryManager::Query {
 public:
  AbstractIntegerQuery(QueryManager* manager,
                       GLenum target,
                       scoped_refptr<gpu::Buffer> buffer,
                       QuerySync* sync);
  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Destroy(bool have_context) override;

 protected:
  ~AbstractIntegerQuery() override;
  bool AreAllResultsAvailable();

  // Service side query ids.
  std::vector<GLuint> service_ids_;
};

AbstractIntegerQuery::AbstractIntegerQuery(QueryManager* manager,
                                           GLenum target,
                                           scoped_refptr<gpu::Buffer> buffer,
                                           QuerySync* sync)
    : Query(manager, target, std::move(buffer), sync) {
  GLuint service_id = 0;
  glGenQueries(1, &service_id);
  DCHECK_NE(0u, service_id);
  service_ids_.push_back(service_id);
}

void AbstractIntegerQuery::Begin() {
  MarkAsActive();
  // Delete all but the first one when beginning a new query.
  if (service_ids_.size() > 1) {
    glDeleteQueries(service_ids_.size() - 1, &service_ids_[1]);
    service_ids_.resize(1);
  }
  BeginQueryHelper(target(), service_ids_.back());
}

void AbstractIntegerQuery::End(base::subtle::Atomic32 submit_count) {
  EndQueryHelper(target());
  AddToPendingQueue(submit_count);
}

void AbstractIntegerQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void AbstractIntegerQuery::Pause() {
  MarkAsPaused();
  EndQueryHelper(target());
}

void AbstractIntegerQuery::Resume() {
  MarkAsActive();

  GLuint service_id = 0;
  glGenQueries(1, &service_id);
  DCHECK_NE(0u, service_id);
  service_ids_.push_back(service_id);
  BeginQueryHelper(target(), service_ids_.back());
}

void AbstractIntegerQuery::Destroy(bool have_context) {
  if (have_context && !IsDeleted()) {
    glDeleteQueries(service_ids_.size(), &service_ids_[0]);
    service_ids_.clear();
    MarkAsDeleted();
  }
}

AbstractIntegerQuery::~AbstractIntegerQuery() = default;

bool AbstractIntegerQuery::AreAllResultsAvailable() {
  GLuint available = 0;
  glGetQueryObjectuiv(service_ids_.back(), GL_QUERY_RESULT_AVAILABLE_EXT,
                      &available);
  return !!available;
}

class BooleanQuery : public AbstractIntegerQuery {
 public:
  BooleanQuery(QueryManager* manager,
               GLenum target,
               scoped_refptr<gpu::Buffer> buffer,
               QuerySync* sync);
  void Process(bool did_finish) override;

 protected:
  ~BooleanQuery() override;
};

BooleanQuery::BooleanQuery(QueryManager* manager,
                           GLenum target,
                           scoped_refptr<gpu::Buffer> buffer,
                           QuerySync* sync)
    : AbstractIntegerQuery(manager, target, std::move(buffer), sync) {}

BooleanQuery::~BooleanQuery() = default;

void BooleanQuery::Process(bool did_finish) {
  if (!AreAllResultsAvailable())
    return;
  for (const GLuint& service_id : service_ids_) {
    GLuint result = 0;
    glGetQueryObjectuiv(service_id, GL_QUERY_RESULT_EXT, &result);
    if (result != 0) {
      MarkAsCompleted(1);
      return;
    }
  }
  MarkAsCompleted(0);
}

class SummedIntegerQuery : public AbstractIntegerQuery {
 public:
  SummedIntegerQuery(QueryManager* manager,
                     GLenum target,
                     scoped_refptr<gpu::Buffer> buffer,
                     QuerySync* sync);
  void Process(bool did_finish) override;

 protected:
  ~SummedIntegerQuery() override;
};

SummedIntegerQuery::SummedIntegerQuery(QueryManager* manager,
                                       GLenum target,
                                       scoped_refptr<gpu::Buffer> buffer,
                                       QuerySync* sync)
    : AbstractIntegerQuery(manager, target, std::move(buffer), sync) {}

SummedIntegerQuery::~SummedIntegerQuery() = default;

void SummedIntegerQuery::Process(bool did_finish) {
  if (!AreAllResultsAvailable())
    return;
  GLuint summed_result = 0;
  for (const GLuint& service_id : service_ids_) {
    GLuint result = 0;
    glGetQueryObjectuiv(service_id, GL_QUERY_RESULT_EXT, &result);
    summed_result += result;
  }
  MarkAsCompleted(summed_result);
}

class CommandLatencyQuery : public QueryManager::Query {
 public:
  CommandLatencyQuery(QueryManager* manager,
                      GLenum target,
                      scoped_refptr<gpu::Buffer> buffer,
                      QuerySync* sync);

  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~CommandLatencyQuery() override;
};

CommandLatencyQuery::CommandLatencyQuery(QueryManager* manager,
                                         GLenum target,
                                         scoped_refptr<gpu::Buffer> buffer,
                                         QuerySync* sync)
    : Query(manager, target, std::move(buffer), sync) {}

void CommandLatencyQuery::Begin() {
  MarkAsActive();
}

void CommandLatencyQuery::Pause() {
  MarkAsPaused();
}

void CommandLatencyQuery::Resume() {
  MarkAsActive();
}

void CommandLatencyQuery::End(base::subtle::Atomic32 submit_count) {
  base::TimeDelta now = base::TimeTicks::Now() - base::TimeTicks();
  MarkAsPending(submit_count);
  MarkAsCompleted(now.InMicroseconds());
}

void CommandLatencyQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void CommandLatencyQuery::Process(bool did_finish) {
  NOTREACHED();
}

void CommandLatencyQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

CommandLatencyQuery::~CommandLatencyQuery() = default;

class AsyncReadPixelsCompletedQuery
    : public GLES2QueryManager::GLES2Query,
      public base::SupportsWeakPtr<AsyncReadPixelsCompletedQuery> {
 public:
  AsyncReadPixelsCompletedQuery(GLES2QueryManager* manager,
                                GLenum target,
                                scoped_refptr<gpu::Buffer> buffer,
                                QuerySync* sync);

  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  void Complete();
  ~AsyncReadPixelsCompletedQuery() override;
};

AsyncReadPixelsCompletedQuery::AsyncReadPixelsCompletedQuery(
    GLES2QueryManager* manager,
    GLenum target,
    scoped_refptr<gpu::Buffer> buffer,
    QuerySync* sync)
    : GLES2Query(manager, target, std::move(buffer), sync) {}

void AsyncReadPixelsCompletedQuery::Begin() {
  MarkAsActive();
}

void AsyncReadPixelsCompletedQuery::Pause() {
  MarkAsPaused();
}

void AsyncReadPixelsCompletedQuery::Resume() {
  MarkAsActive();
}

void AsyncReadPixelsCompletedQuery::End(base::subtle::Atomic32 submit_count) {
  MarkAsPending(submit_count);
  gles2_query_manager()->decoder()->WaitForReadPixels(
      base::BindOnce(&AsyncReadPixelsCompletedQuery::Complete, AsWeakPtr()));
}

void AsyncReadPixelsCompletedQuery::QueryCounter(
    base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void AsyncReadPixelsCompletedQuery::Complete() {
  MarkAsCompleted(1);
}

void AsyncReadPixelsCompletedQuery::Process(bool did_finish) {
  NOTREACHED();
}

void AsyncReadPixelsCompletedQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

AsyncReadPixelsCompletedQuery::~AsyncReadPixelsCompletedQuery() = default;

class GetErrorQuery : public GLES2QueryManager::GLES2Query {
 public:
  GetErrorQuery(GLES2QueryManager* manager,
                GLenum target,
                scoped_refptr<gpu::Buffer> buffer,
                QuerySync* sync);

  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~GetErrorQuery() override;
};

GetErrorQuery::GetErrorQuery(GLES2QueryManager* manager,
                             GLenum target,
                             scoped_refptr<gpu::Buffer> buffer,
                             QuerySync* sync)
    : GLES2Query(manager, target, std::move(buffer), sync) {}

void GetErrorQuery::Begin() {
  MarkAsActive();
}

void GetErrorQuery::Pause() {
  MarkAsPaused();
}

void GetErrorQuery::Resume() {
  MarkAsActive();
}

void GetErrorQuery::End(base::subtle::Atomic32 submit_count) {
  MarkAsPending(submit_count);
  MarkAsCompleted(
      gles2_query_manager()->decoder()->GetErrorState()->GetGLError());
}

void GetErrorQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void GetErrorQuery::Process(bool did_finish) {
  NOTREACHED();
}

void GetErrorQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

GetErrorQuery::~GetErrorQuery() = default;

class TimeElapsedQuery : public GLES2QueryManager::GLES2Query {
 public:
  TimeElapsedQuery(GLES2QueryManager* manager,
                   GLenum target,
                   scoped_refptr<gpu::Buffer> buffer,
                   QuerySync* sync);

  // Overridden from QueryManager::Query:
  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~TimeElapsedQuery() override;

 private:
  std::unique_ptr<gl::GPUTimer> gpu_timer_;
};

TimeElapsedQuery::TimeElapsedQuery(GLES2QueryManager* manager,
                                   GLenum target,
                                   scoped_refptr<gpu::Buffer> buffer,
                                   QuerySync* sync)
    : GLES2Query(manager, target, std::move(buffer), sync),
      gpu_timer_(manager->CreateGPUTimer(true)) {}

void TimeElapsedQuery::Begin() {
  // Reset the disjoint value before the query begins if it is safe.
  SafelyResetDisjointValue();
  MarkAsActive();
  gpu_timer_->Start();
}

void TimeElapsedQuery::End(base::subtle::Atomic32 submit_count) {
  gpu_timer_->End();
  AddToPendingQueue(submit_count);
}

void TimeElapsedQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void TimeElapsedQuery::Pause() {
  MarkAsPaused();
}

void TimeElapsedQuery::Resume() {
  MarkAsActive();
}

void TimeElapsedQuery::Process(bool did_finish) {
  if (!gpu_timer_->IsAvailable())
    return;

  // Make sure disjoint value is up to date. This disjoint check is the only one
  // that needs to be done to validate that this query is valid. If a disjoint
  // occurs before the client checks the query value we will just hide the
  // disjoint state since it did not affect this query.
  UpdateDisjointValue();

  const uint64_t nano_seconds =
      gpu_timer_->GetDeltaElapsed() * base::Time::kNanosecondsPerMicrosecond;
  MarkAsCompleted(nano_seconds);
}

void TimeElapsedQuery::Destroy(bool have_context) {
  gpu_timer_->Destroy(have_context);
}

TimeElapsedQuery::~TimeElapsedQuery() = default;

class TimeStampQuery : public GLES2QueryManager::GLES2Query {
 public:
  TimeStampQuery(GLES2QueryManager* manager,
                 GLenum target,
                 scoped_refptr<gpu::Buffer> buffer,
                 QuerySync* sync);

  // Overridden from QueryManager::Query:
  void Begin() override;
  void End(base::subtle::Atomic32 submit_count) override;
  void QueryCounter(base::subtle::Atomic32 submit_count) override;
  void Pause() override;
  void Resume() override;
  void Process(bool did_finish) override;
  void Destroy(bool have_context) override;

 protected:
  ~TimeStampQuery() override;

 private:
  std::unique_ptr<gl::GPUTimer> gpu_timer_;
};

TimeStampQuery::TimeStampQuery(GLES2QueryManager* manager,
                               GLenum target,
                               scoped_refptr<gpu::Buffer> buffer,
                               QuerySync* sync)
    : GLES2Query(manager, target, std::move(buffer), sync),
      gpu_timer_(manager->CreateGPUTimer(false)) {}

void TimeStampQuery::Begin() {
  NOTREACHED();
}

void TimeStampQuery::End(base::subtle::Atomic32 submit_count) {
  NOTREACHED();
}

void TimeStampQuery::Pause() {
  MarkAsPaused();
}

void TimeStampQuery::Resume() {
  MarkAsActive();
}

void TimeStampQuery::QueryCounter(base::subtle::Atomic32 submit_count) {
  // Reset the disjoint value before the query begins if it is safe.
  SafelyResetDisjointValue();
  MarkAsActive();
  // After a timestamp has begun, we will want to continually detect
  // the disjoint value every frame until the context is destroyed.
  BeginContinualDisjointUpdate();

  gpu_timer_->QueryTimeStamp();
  AddToPendingQueue(submit_count);
}

void TimeStampQuery::Process(bool did_finish) {
  if (!gpu_timer_->IsAvailable())
    return;

  // Make sure disjoint value is up to date. This disjoint check is the only one
  // that needs to be done to validate that this query is valid. If a disjoint
  // occurs before the client checks the query value we will just hide the
  // disjoint state since it did not affect this query.
  UpdateDisjointValue();

  int64_t start = 0;
  int64_t end = 0;
  gpu_timer_->GetStartEndTimestamps(&start, &end);
  DCHECK(start == end);

  const uint64_t nano_seconds = start * base::Time::kNanosecondsPerMicrosecond;
  MarkAsCompleted(nano_seconds);
}

void TimeStampQuery::Destroy(bool have_context) {
  if (gpu_timer_.get()) {
    gpu_timer_->Destroy(have_context);
    gpu_timer_.reset();
  }
}

TimeStampQuery::~TimeStampQuery() = default;

}  // namespace

GLES2QueryManager::GLES2Query::GLES2Query(GLES2QueryManager* manager,
                                          GLenum target,
                                          scoped_refptr<gpu::Buffer> buffer,
                                          QuerySync* sync)
    : QueryManager::Query(manager, target, buffer, sync),
      gles2_query_manager_(manager) {}

GLES2QueryManager::GLES2Query::~GLES2Query() = default;

GLES2QueryManager::GLES2QueryManager(GLES2Decoder* decoder,
                                     FeatureInfo* feature_info)
    : decoder_(decoder),
      use_arb_occlusion_query2_for_occlusion_query_boolean_(
          feature_info->feature_flags()
              .use_arb_occlusion_query2_for_occlusion_query_boolean),
      use_arb_occlusion_query_for_occlusion_query_boolean_(
          feature_info->feature_flags()
              .use_arb_occlusion_query_for_occlusion_query_boolean),
      update_disjoints_continually_(false),
      disjoint_notify_shm_id_(-1),
      disjoint_notify_shm_offset_(0),
      disjoints_notified_(0) {
  DCHECK(!(use_arb_occlusion_query_for_occlusion_query_boolean_ &&
           use_arb_occlusion_query2_for_occlusion_query_boolean_));
  DCHECK(decoder);
  gl::GLContext* context = decoder_->GetGLContext();
  if (context) {
    gpu_timing_client_ = context->CreateGPUTimingClient();
  } else {
    gpu_timing_client_ = new gl::GPUTimingClient();
  }
}

GLES2QueryManager::~GLES2QueryManager() = default;

QueryManager::Query* GLES2QueryManager::CreateQuery(
    GLenum target,
    GLuint client_id,
    scoped_refptr<gpu::Buffer> buffer,
    QuerySync* sync) {
  scoped_refptr<Query> query;
  switch (target) {
    case GL_LATENCY_QUERY_CHROMIUM:
      query = new CommandLatencyQuery(this, target, std::move(buffer), sync);
      break;
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
      query = new AsyncReadPixelsCompletedQuery(this, target, std::move(buffer),
                                                sync);
      break;
    case GL_GET_ERROR_QUERY_CHROMIUM:
      query = new GetErrorQuery(this, target, std::move(buffer), sync);
      break;
    case GL_TIME_ELAPSED:
      query = new TimeElapsedQuery(this, target, std::move(buffer), sync);
      break;
    case GL_TIMESTAMP:
      query = new TimeStampQuery(this, target, std::move(buffer), sync);
      break;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
      query = new BooleanQuery(this, target, std::move(buffer), sync);
      break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
      query = new SummedIntegerQuery(this, target, std::move(buffer), sync);
      break;
    case GL_SAMPLES_PASSED:
      query = new SummedIntegerQuery(this, target, std::move(buffer), sync);
      break;
    default:
      return QueryManager::CreateQuery(target, client_id, buffer, sync);
  }
  std::pair<QueryMap::iterator, bool> result =
      queries_.insert(std::make_pair(client_id, query));
  DCHECK(result.second);
  return query.get();
}

void GLES2QueryManager::ProcessFrameBeginUpdates() {
  if (update_disjoints_continually_)
    UpdateDisjointValue();
}

error::Error GLES2QueryManager::SetDisjointSync(int32_t shm_id,
                                                uint32_t shm_offset) {
  if (disjoint_notify_shm_id_ != -1 || shm_id == -1)
    return error::kInvalidArguments;

  DisjointValueSync* sync = decoder_->GetSharedMemoryAs<DisjointValueSync*>(
      shm_id, shm_offset, sizeof(*sync));
  if (!sync)
    return error::kOutOfBounds;

  sync->Reset();
  disjoints_notified_ = 0;

  disjoint_notify_shm_id_ = shm_id;
  disjoint_notify_shm_offset_ = shm_offset;
  return error::kNoError;
}

std::unique_ptr<gl::GPUTimer> GLES2QueryManager::CreateGPUTimer(
    bool elapsed_time) {
  return gpu_timing_client_->CreateGPUTimer(elapsed_time);
}

bool GLES2QueryManager::GPUTimingAvailable() {
  return gpu_timing_client_->IsAvailable();
}

GLenum GLES2QueryManager::AdjustTargetForEmulation(GLenum target) {
  switch (target) {
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
    case GL_ANY_SAMPLES_PASSED_EXT:
      if (use_arb_occlusion_query2_for_occlusion_query_boolean_) {
        // ARB_occlusion_query2 does not have a
        // GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT
        // target.
        target = GL_ANY_SAMPLES_PASSED_EXT;
      } else if (use_arb_occlusion_query_for_occlusion_query_boolean_) {
        // ARB_occlusion_query does not have a
        // GL_ANY_SAMPLES_PASSED_EXT
        // target.
        target = GL_SAMPLES_PASSED_ARB;
      }
      break;
    default:
      break;
  }
  return target;
}

void GLES2QueryManager::UpdateDisjointValue() {
  if (disjoint_notify_shm_id_ != -1) {
    if (gpu_timing_client_->CheckAndResetTimerErrors()) {
      disjoints_notified_++;

      DisjointValueSync* sync = decoder_->GetSharedMemoryAs<DisjointValueSync*>(
          disjoint_notify_shm_id_, disjoint_notify_shm_offset_, sizeof(*sync));
      if (!sync) {
        // Shared memory does not seem to be valid, ignore the shm id/offset.
        disjoint_notify_shm_id_ = -1;
        disjoint_notify_shm_offset_ = 0;
      } else {
        sync->SetDisjointCount(disjoints_notified_);
      }
    }
  }
}

void GLES2QueryManager::SafelyResetDisjointValue() {
  // It is only safe to reset the disjoint value is there is no active
  // elapsed timer and we are not continually updating the disjoint value.
  if (!update_disjoints_continually_ && !GetActiveQuery(GL_TIME_ELAPSED)) {
    // Reset the error state without storing the result.
    gpu_timing_client_->CheckAndResetTimerErrors();
  }
}

}  // namespace gles2
}  // namespace gpu
