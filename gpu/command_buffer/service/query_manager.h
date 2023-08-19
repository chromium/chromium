// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/atomicops.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/common_cmd_format.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// This class keeps track of the queries and their state
// As Queries are not shared there is one QueryManager per context.
class GPU_GLES2_EXPORT QueryManager {
 public:
  class GPU_GLES2_EXPORT Query : public base::RefCounted<Query> {
   public:
    Query(QueryManager* manager,
          GLenum target,
          scoped_refptr<gpu::Buffer> buffer,
          QuerySync* sync);

    GLenum target() const {
      return target_;
    }

    bool IsDeleted() const {
      return deleted_;
    }

    bool IsValid() const {
      return target() && !IsDeleted();
    }

    bool IsActive() const {
      return query_state_ == kQueryState_Active;
    }

    bool IsPaused() const {
      return query_state_ == kQueryState_Paused;
    }

    bool IsPending() const {
      return query_state_ == kQueryState_Pending;
    }

    bool IsFinished() const {
      return query_state_ == kQueryState_Finished;
    }

    const QuerySync* sync() const { return sync_; }

    virtual void Begin() = 0;

    virtual void End(base::subtle::Atomic32 submit_count) = 0;

    virtual void QueryCounter(base::subtle::Atomic32 submit_count) = 0;

    virtual void Process(bool did_finish) = 0;

    // Pauses active query to be resumed later.
    virtual void Pause() = 0;

    // Resume from a paused active query.
    virtual void Resume() = 0;

    virtual void Destroy(bool have_context) = 0;

    virtual void BeginProcessingCommands() {}
    virtual void EndProcessingCommands() {}

    void AddCallback(base::OnceClosure callback);

   protected:
    virtual ~Query();

    QueryManager* manager() const {
      return manager_;
    }

    void MarkAsDeleted() {
      deleted_ = true;
    }

    void MarkAsActive() {
      DCHECK(query_state_ == kQueryState_Initialize ||
             query_state_ == kQueryState_Paused ||
             query_state_ == kQueryState_Finished);
      query_state_ = kQueryState_Active;
    }

    void MarkAsPaused() {
      DCHECK(query_state_ == kQueryState_Active);
      query_state_ = kQueryState_Paused;
    }

    void MarkAsPending(base::subtle::Atomic32 submit_count) {
      DCHECK(query_state_ == kQueryState_Active);
      query_state_ = kQueryState_Pending;
      submit_count_ = submit_count;
    }

    void MarkAsCompleted(uint64_t result);

    void UnmarkAsPending() {
      DCHECK(query_state_ == kQueryState_Pending);
      query_state_ = kQueryState_Finished;
    }

    void AddToPendingQueue(base::subtle::Atomic32 submit_count) {
      manager_->AddPendingQuery(this, submit_count);
    }

    void BeginQueryHelper(GLenum target, GLuint id) {
      manager_->BeginQueryHelper(target, id);
    }

    void EndQueryHelper(GLenum target) {
      manager_->EndQueryHelper(target);
    }

    base::subtle::Atomic32 submit_count() const { return submit_count_; }

   private:
    friend class QueryManager;
    friend class QueryManagerTest;
    friend class base::RefCounted<Query>;

    void RunCallbacks();

    // The manager that owns this Query.
    raw_ptr<QueryManager> manager_;

    // The type of query.
    GLenum target_;

    // The shared memory used with this Query. We keep a reference to the Buffer
    // to ensure it doesn't get released until we wrote results. sync_ points to
    // memory inside buffer_.
    scoped_refptr<gpu::Buffer> buffer_;
    raw_ptr<QuerySync> sync_;

    // Count to set process count do when completed.
    base::subtle::Atomic32 submit_count_;

    // Current state of the query.
    enum QueryState {
      kQueryState_Initialize, // Has not been queried yet.
      kQueryState_Active, // Query began but has not ended.
      kQueryState_Paused, // Query was active but is now paused.
      kQueryState_Pending, // Query ended, waiting for result.
      kQueryState_Finished, // Query received result.
    } query_state_;

    // True if deleted.
    bool deleted_;

    // List of callbacks to run when result is available.
    std::vector<base::OnceClosure> callbacks_;
  };

  QueryManager();

  QueryManager(const QueryManager&) = delete;
  QueryManager& operator=(const QueryManager&) = delete;

  virtual ~QueryManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Query for the given query.
  virtual Query* CreateQuery(GLenum target,
                             GLuint client_id,
                             scoped_refptr<gpu::Buffer> buffer,
                             QuerySync* sync);

  // Gets the query info for the given query.
  Query* GetQuery(GLuint client_id);

  // Gets the currently active query for a target.
  Query* GetActiveQuery(GLenum target);

  // Removes a query info for the given query.
  void RemoveQuery(GLuint client_id);

  // Removes a query info for all pending queries.
  void RemoveAllQueries();

  // Returns false if any query is pointing to invalid shared memory.
  void BeginQuery(Query* query);

  // Returns false if any query is pointing to invalid shared memory.
  void EndQuery(Query* query, base::subtle::Atomic32 submit_count);

  // Returns false if any query is pointing to invalid shared memory.
  void QueryCounter(Query* query, base::subtle::Atomic32 submit_count);

  void PauseQueries();
  void ResumeQueries();

  void BeginProcessingCommands();
  void EndProcessingCommands();

  // Processes pending queries. Returns false if any queries are pointing
  // to invalid shared memory. |did_finish| is true if this is called as
  // a result of calling glFinish().
  void ProcessPendingQueries(bool did_finish);

  // True if there are pending queries.
  bool HavePendingQueries();

  void GenQueries(GLsizei n, const GLuint* queries);
  bool IsValidQuery(GLuint id);

 protected:
  void StartTracking(Query* query);
  void StopTracking(Query* query);

  // Wrappers for BeginQueryARB and EndQueryARB to hide differences between
  // ARB_occlusion_query2 and EXT_occlusion_query_boolean.
  void BeginQueryHelper(GLenum target, GLuint id);
  void EndQueryHelper(GLenum target);

  // Adds to queue of queries waiting for completion.
  // Returns false if any query is pointing to invalid shared memory.
  void AddPendingQuery(Query* query, base::subtle::Atomic32 submit_count);

  // Removes a query from the queue of pending queries.
  // Returns false if any query is pointing to invalid shared memory.
  void RemovePendingQuery(Query* query);

  // Returns a target used for the underlying GL extension
  // used to emulate a query.
  virtual GLenum AdjustTargetForEmulation(GLenum target);

  // Counts the number of Queries allocated with 'this' as their manager.
  // Allows checking no Query will outlive this.
  unsigned query_count_;

  // Info for each query in the system.
  using QueryMap = std::unordered_map<GLuint, scoped_refptr<Query>>;
  QueryMap queries_;

  using GeneratedQueryIds = std::unordered_set<GLuint>;
  GeneratedQueryIds generated_query_ids_;

  // A map of targets -> Query for current active queries.
  using ActiveQueryMap = std::map<GLenum, scoped_refptr<Query>>;
  ActiveQueryMap active_queries_;

  // Queries waiting for completion.
  using QueryQueue = base::circular_deque<scoped_refptr<Query>>;
  QueryQueue pending_queries_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_
