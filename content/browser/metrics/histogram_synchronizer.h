// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/metrics/histogram_subscriber.h"

namespace content {

// This class maintains state that is used to upload histogram data from the
// various child processes, into the browser process. Such transactions are
// usually instigated by the browser. In general, a child process will respond
// by gathering snapshots of all internal histograms, calculating what has
// changed since its last upload, and transmitting a pickled collection of
// deltas.
//
// There are actually two modes of update request.  One is synchronous (and
// blocks the UI thread, waiting to populate an about:histograms tab) and the
// other is asynchronous, and used by the metrics services in preparation for a
// log upload.
//
// To assure that all the processes have responded, a counter is maintained to
// indicate the number of pending (not yet responsive) processes. To avoid
// confusion about a response (i.e., is the process responding to a current
// request for an update, or to an old request for an update) we tag each group
// of requests with a sequence number. When an update arrives we can ignore it
// (relative to the counter) if it does not relate to a current outstanding
// sequence number.
//
// There is one final mode of use, where a renderer spontaneously decides to
// transmit a collection of histogram data.  This is designed for use when the
// renderer is terminating.  Unfortunately, renders may be terminated without
// warning, and the best we can do is periodically acquire data from a tab, such
// as when a page load has completed.  In this mode, the renderer uses a
// reserved sequence number, different from any sequence number that might be
// specified by a browser request.  Since this sequence number can't match an
// outstanding sequence number, the pickled data is accepted into the browser,
// but there is no impact on the counters.

class HistogramSynchronizer : public metrics::HistogramSubscriber {
 public:
  enum ProcessHistogramRequester {
    UNKNOWN,
    ASYNC_HISTOGRAMS,
  };

  // Return pointer to the singleton instance for the current process, or NULL
  // if none.
  static HistogramSynchronizer* GetInstance();

  HistogramSynchronizer(const HistogramSynchronizer&) = delete;
  HistogramSynchronizer& operator=(const HistogramSynchronizer&) = delete;

  // Contact all processes, and get them to upload to the browser any/all
  // changes to histograms. This method is called from about:histograms.
  static void FetchHistograms();

  // Contact all child processes, and get them to upload to the browser any/all
  // changes to histograms.  When all changes have been acquired, or when the
  // wait time expires (whichever is sooner), post the callback to the specified
  // TaskRunner. Note the callback is posted exactly once.
  static void FetchHistogramsAsynchronously(
      scoped_refptr<base::TaskRunner> task_runner,
      base::OnceClosure callback,
      base::TimeDelta wait_time);

 private:
  friend struct base::DefaultSingletonTraits<HistogramSynchronizer>;

  class RequestContext;

  HistogramSynchronizer();
  ~HistogramSynchronizer() override;

  // Establish a new sequence number, and use it to notify all processes
  // (renderers, plugins, GPU, etc) of the need to supply, to the browser,
  // any/all changes to their histograms. |wait_time| specifies the amount of
  // time to wait before cancelling the requests for non-responsive processes.
  void RegisterAndNotifyAllProcesses(ProcessHistogramRequester requester,
                                     base::TimeDelta wait_time);

  // -------------------------------------------------------
  // HistogramSubscriber methods for browser child processes
  // -------------------------------------------------------

  // Update the number of pending processes for the given |sequence_number|.
  // This is called on UI thread.
  void OnPendingProcesses(int sequence_number,
                          int pending_processes,
                          bool end) override;

  // Send histogram_data back to caller and also record that we are waiting
  // for one less histogram data from child process for the given sequence
  // number. This method is accessible on UI thread.
  void OnHistogramDataCollected(
      int sequence_number,
      const std::vector<std::string>& pickled_histograms) override;

  // Set the |callback_task_runner_| and |callback_| members. If these members
  // already had values, then as a side effect, post the old |callback_| to the
  // old |callback_task_runner_|. This side effect should not generally happen,
  // but is in place to assure correctness (that any tasks that were set, are
  // eventually called, and never merely discarded).
  void SetTaskRunnerAndCallback(scoped_refptr<base::TaskRunner> task_runner,
                                base::OnceClosure callback);

  void ForceHistogramSynchronizationDoneCallback(int sequence_number);

  // Internal helper function, to post task, and record callback stats.
  void InternalPostTask(scoped_refptr<base::TaskRunner> task_runner,
                        base::OnceClosure callback);

  // Gets a new sequence number to be sent to processes from browser process.
  int GetNextAvailableSequenceNumber(ProcessHistogramRequester requester);

  // This lock_ protects access to all members.
  base::Lock lock_;

  // When a request is made to asynchronously update the histograms, we store
  // the task and TaskRunner we use to post a completion notification in
  // |callback_| and |callback_task_runner_|.
  base::OnceClosure callback_ GUARDED_BY(lock_);
  scoped_refptr<base::TaskRunner> callback_task_runner_ GUARDED_BY(lock_);

  // We don't track the actual processes that are contacted for an update, only
  // the count of the number of processes, and we can sometimes time-out and
  // give up on a "slow to respond" process.  We use a sequence_number to be
  // sure a response from a process is associated with the current round of
  // requests (and not merely a VERY belated prior response).
  // All sequence numbers used are non-negative.
  // last_used_sequence_number_ is the most recently used number (used to avoid
  // reuse for a long time).
  int last_used_sequence_number_ GUARDED_BY(lock_);

  // The sequence number used by the most recent asynchronous update request to
  // contact all processes.
  int async_sequence_number_ GUARDED_BY(lock_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_
