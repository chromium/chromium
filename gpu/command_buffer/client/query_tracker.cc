// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/query_tracker.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/atomicops.h"
#include "base/containers/circular_deque.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/time.h"

namespace gpu {
namespace gles2 {

QuerySyncManager::Bucket::Bucket(QuerySync* sync_mem,
                                 int32_t shm_id,
                                 unsigned int shm_offset)
    : syncs(sync_mem), shm_id(shm_id), base_shm_offset(shm_offset) {}

QuerySyncManager::Bucket::~Bucket() = default;

void QuerySyncManager::Bucket::FreePendingSyncs() {
  base::EraseIf(pending_syncs, [this](const PendingSync& pending) {
    QuerySync* sync = this->syncs + pending.index;
    if (base::subtle::Acquire_Load(&sync->process_count) ==
        pending.submit_count) {
      this->in_use_query_syncs[pending.index] = false;
      return true;
    } else {
      return false;
    }
  });
}

QuerySyncManager::QuerySyncManager(MappedMemoryManager* manager)
    : mapped_memory_(manager) {
  DCHECK(manager);
}

QuerySyncManager::~QuerySyncManager() {
  while (!buckets_.empty()) {
    mapped_memory_->Free(buckets_.front()->syncs);
    buckets_.pop_front();
  }
}

bool QuerySyncManager::Alloc(QuerySyncManager::QueryInfo* info) {
  DCHECK(info);
  Bucket* bucket = nullptr;
  for (auto& bucket_candidate : buckets_) {
    bucket_candidate->FreePendingSyncs();
    if (!bucket_candidate->in_use_query_syncs.all()) {
      bucket = bucket_candidate.get();
      break;
    }
  }
  if (!bucket) {
    int32_t shm_id;
    unsigned int shm_offset;
    void* mem = mapped_memory_->Alloc(
        kSyncsPerBucket * sizeof(QuerySync), &shm_id, &shm_offset);
    if (!mem) {
      return false;
    }
    QuerySync* syncs = static_cast<QuerySync*>(mem);
    buckets_.push_back(std::make_unique<Bucket>(syncs, shm_id, shm_offset));
    bucket = buckets_.back().get();
  }

  uint32_t index_in_bucket = 0;
  for (uint32_t i = 0; i < kSyncsPerBucket; i++) {
    if (!bucket->in_use_query_syncs[i]) {
      index_in_bucket = i;
      break;
    }
  }

  *info = QueryInfo(bucket, index_in_bucket);
  info->sync->Reset();
  bucket->in_use_query_syncs[index_in_bucket] = true;
  return true;
}

void QuerySyncManager::Free(const QuerySyncManager::QueryInfo& info) {
  DCHECK_NE(info.bucket->in_use_query_syncs.count(), 0u);
  unsigned short index_in_bucket = info.index();
  DCHECK(info.bucket->in_use_query_syncs[index_in_bucket]);
  if (base::subtle::Acquire_Load(&info.sync->process_count) !=
      info.submit_count) {
    // When you delete a query you can't mark its memory as unused until it's
    // completed.
    info.bucket->pending_syncs.push_back(
        Bucket::PendingSync{index_in_bucket, info.submit_count});
  } else {
    info.bucket->in_use_query_syncs[index_in_bucket] = false;
  }
}

void QuerySyncManager::Shrink(CommandBufferHelper* helper) {
  base::circular_deque<std::unique_ptr<Bucket>> new_buckets;
  uint32_t token = 0;
  while (!buckets_.empty()) {
    std::unique_ptr<Bucket>& bucket = buckets_.front();
    bucket->FreePendingSyncs();
    if (bucket->in_use_query_syncs.any()) {
      if (bucket->in_use_query_syncs.count() == bucket->pending_syncs.size()) {
        // Every QuerySync that is in-use is just pending completion. We know
        // the query has been deleted, so nothing on the service side will
        // access the shared memory after current commands, so we can
        // free-pending-token.
        token = helper->InsertToken();
        mapped_memory_->FreePendingToken(bucket->syncs, token);
      } else {
        new_buckets.push_back(std::move(bucket));
      }
    } else {
      // Every QuerySync is free or completed, so we know the service side won't
      // access it any more, so we can free immediately.
      mapped_memory_->Free(bucket->syncs);
    }
    buckets_.pop_front();
  }
  buckets_.swap(new_buckets);
}

QueryTracker::Query::Query(GLuint id,
                           GLenum target,
                           const QuerySyncManager::QueryInfo& info)
    : id_(id),
      target_(target),
      info_(info),
      state_(kUninitialized),
      token_(0),
      flush_count_(0),
      client_begin_time_us_(0),
      result_(0) {}

QueryTracker::Query::~Query() {}

void QueryTracker::Query::Begin(QueryTrackerClient* client) {
  // init memory, inc count
  MarkAsActive();

  switch (target()) {
    case GL_GET_ERROR_QUERY_CHROMIUM:
      // To nothing on begin for error queries.
      break;
    case GL_LATENCY_QUERY_CHROMIUM:
      client_begin_time_us_ = MicrosecondsSinceOriginOfTime();
      // tell service about id, shared memory and count
      client->IssueBeginQuery(target(), id(), shm_id(), shm_offset());
      break;
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    default:
      // tell service about id, shared memory and count
      client->IssueBeginQuery(target(), id(), shm_id(), shm_offset());
      break;
  }
}

void QueryTracker::Query::End(QueryTrackerClient* client) {
  switch (target()) {
    case GL_GET_ERROR_QUERY_CHROMIUM: {
      GLenum error = client->GetClientSideGLError();
      if (error == GL_NO_ERROR) {
        // There was no error so start the query on the service.
        // it will end immediately.
        client->IssueBeginQuery(target(), id(), shm_id(), shm_offset());
      } else {
        // There's an error on the client, no need to bother the service. Just
        // set the query as completed and return the error.
        if (error != GL_NO_ERROR) {
          state_ = kComplete;
          result_ = error;
          return;
        }
      }
    }
  }
  flush_count_ = client->cmd_buffer_helper()->flush_generation();
  int32_t submit_count = NextSubmitCount();
  client->IssueEndQuery(target(), submit_count);
  MarkAsPending(client->cmd_buffer_helper()->InsertToken(), submit_count);
}

void QueryTracker::Query::QueryCounter(QueryTrackerClient* client) {
  MarkAsActive();
  flush_count_ = client->cmd_buffer_helper()->flush_generation();
  int32_t submit_count = NextSubmitCount();
  client->IssueQueryCounter(id(), target(), shm_id(), shm_offset(),
                            submit_count);
  MarkAsPending(client->cmd_buffer_helper()->InsertToken(), submit_count);
}

bool QueryTracker::Query::CheckResultsAvailable(CommandBufferHelper* helper,
                                                bool flush_if_pending) {
  if (Pending()) {
    bool processed_all = base::subtle::Acquire_Load(
                             &info_.sync->process_count) == submit_count();
    // We check lost on the command buffer itself here instead of checking the
    // QueryTrackerClient because the QueryTrackerClient will not hear about
    // the loss until we exit out of this call stack (to avoid re-entrancy), and
    // we need be able to enter kComplete state on context loss.
    // TODO(danakj): If QueryTrackerClient can handle being notified of loss
    // re-entrantly (without calling its clients re-entrantly), then we could
    // call QueryTrackerClient::GetGraphicsResetStatusKHR() here and remove
    // this method from CommandBufferHelper.
    if (processed_all || helper->IsContextLost()) {
      switch (target()) {
        case GL_LATENCY_QUERY_CHROMIUM:
          // Disabled DCHECK because of http://crbug.com/419236.
          //DCHECK(info_.sync->result >= client_begin_time_us_);
          result_ = info_.sync->result - client_begin_time_us_;
          break;
        case GL_COMMANDS_ISSUED_CHROMIUM:
        case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
        case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
        case GL_GET_ERROR_QUERY_CHROMIUM:
        case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
        case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
        case GL_COMMANDS_COMPLETED_CHROMIUM:
        case GL_SAMPLES_PASSED_ARB:
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case GL_TIME_ELAPSED_EXT:
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        case GL_TIMESTAMP_EXT:
          result_ = info_.sync->result;
          break;
        default:
          NOTREACHED();
      }
      if (on_completed_callback_) {
        std::move(on_completed_callback_.value()).Run();
      }
      state_ = kComplete;
    } else {
      if (flush_if_pending &&
          (helper->flush_generation() - flush_count_ - 1) >= 0x80000000) {
        helper->Flush();
      } else {
        // Insert no-ops so that eventually the GPU process will see more work.
        helper->Noop(1);
      }
    }
  }
  return state_ == kComplete;
}

uint64_t QueryTracker::Query::GetResult() const {
  DCHECK(state_ == kComplete || state_ == kUninitialized);
  return result_;
}

void QueryTracker::Query::SetCompletedCallback(base::OnceClosure callback) {
  DCHECK(!on_completed_callback_);
  DCHECK(state_ == kPending);
  on_completed_callback_ = std::move(callback);
}

QueryTracker::QueryTracker(MappedMemoryManager* manager)
    : query_sync_manager_(manager),
      mapped_memory_(manager),
      disjoint_count_sync_shm_id_(-1),
      disjoint_count_sync_shm_offset_(0),
      disjoint_count_sync_(nullptr),
      local_disjoint_count_(0) {
}

QueryTracker::~QueryTracker() {
  for (auto& kv : queries_)
    query_sync_manager_.Free(kv.second->info_);
  if (disjoint_count_sync_) {
    mapped_memory_->Free(disjoint_count_sync_);
    disjoint_count_sync_ = nullptr;
  }
}

QueryTracker::Query* QueryTracker::CreateQuery(GLuint id, GLenum target) {
  DCHECK_NE(0u, id);
  QuerySyncManager::QueryInfo info;
  if (!query_sync_manager_.Alloc(&info)) {
    return nullptr;
  }
  auto query = std::make_unique<Query>(id, target, info);
  Query* query_ptr = query.get();
  std::pair<QueryIdMap::iterator, bool> result =
      queries_.emplace(id, std::move(query));
  DCHECK(result.second);
  return query_ptr;
}

QueryTracker::Query* QueryTracker::GetQuery(GLuint client_id) {
  QueryIdMap::iterator it = queries_.find(client_id);
  return it != queries_.end() ? it->second.get() : nullptr;
}

QueryTracker::Query* QueryTracker::GetCurrentQuery(GLenum target) {
  QueryTargetMap::iterator it = current_queries_.find(target);
  return it != current_queries_.end() ? it->second : nullptr;
}

void QueryTracker::RemoveQuery(GLuint client_id) {
  QueryIdMap::iterator it = queries_.find(client_id);
  if (it != queries_.end()) {
    Query* query = it->second.get();

    // Erase from current targets map if it is the current target.
    const GLenum target = query->target();
    QueryTargetMap::iterator target_it = current_queries_.find(target);
    if (target_it != current_queries_.end() && target_it->second == query) {
      current_queries_.erase(target_it);
    }

    query_sync_manager_.Free(query->info_);
    queries_.erase(it);
  }
}

void QueryTracker::Shrink(CommandBufferHelper* helper) {
  query_sync_manager_.Shrink(helper);
}

bool QueryTracker::BeginQuery(GLuint id,
                              GLenum target,
                              QueryTrackerClient* client) {
  QueryTracker::Query* query = GetQuery(id);
  if (!query) {
    query = CreateQuery(id, target);
    if (!query) {
      client->SetGLError(GL_OUT_OF_MEMORY, "glBeginQueryEXT",
                         "transfer buffer allocation failed");
      return false;
    }
  } else if (query->target() != target) {
    client->SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                       "target does not match");
    return false;
  }

  current_queries_[query->target()] = query;
  query->Begin(client);
  return true;
}

bool QueryTracker::EndQuery(GLenum target, QueryTrackerClient* client) {
  QueryTargetMap::iterator target_it = current_queries_.find(target);
  if (target_it == current_queries_.end()) {
    client->SetGLError(GL_INVALID_OPERATION, "glEndQueryEXT",
                       "no active query");
    return false;
  }

  target_it->second->End(client);
  current_queries_.erase(target_it);
  return true;
}

bool QueryTracker::QueryCounter(GLuint id,
                                GLenum target,
                                QueryTrackerClient* client) {
  QueryTracker::Query* query = GetQuery(id);
  if (!query) {
    query = CreateQuery(id, target);
    if (!query) {
      client->SetGLError(GL_OUT_OF_MEMORY, "glQueryCounterEXT",
                         "transfer buffer allocation failed");
      return false;
    }
  } else if (query->target() != target) {
    client->SetGLError(GL_INVALID_OPERATION, "glQueryCounterEXT",
                       "target does not match");
    return false;
  }

  query->QueryCounter(client);
  return true;
}

bool QueryTracker::SetDisjointSync(QueryTrackerClient* client) {
  if (!disjoint_count_sync_) {
    // Allocate memory for disjoint value sync.
    int32_t shm_id = -1;
    uint32_t shm_offset;
    void* mem = mapped_memory_->Alloc(sizeof(*disjoint_count_sync_),
                                      &shm_id,
                                      &shm_offset);
    if (mem) {
      disjoint_count_sync_shm_id_ = shm_id;
      disjoint_count_sync_shm_offset_ = shm_offset;
      disjoint_count_sync_ = static_cast<DisjointValueSync*>(mem);
      disjoint_count_sync_->Reset();
      client->IssueSetDisjointValueSync(shm_id, shm_offset);
    }
  }
  return disjoint_count_sync_ != nullptr;
}

bool QueryTracker::CheckAndResetDisjoint() {
  if (disjoint_count_sync_) {
    const uint32_t disjoint_count = disjoint_count_sync_->GetDisjointCount();
    if (local_disjoint_count_ != disjoint_count) {
      local_disjoint_count_ = disjoint_count;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu
