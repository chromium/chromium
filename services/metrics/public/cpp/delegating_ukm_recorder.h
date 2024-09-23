// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_

#include <set>

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {

namespace internal {
class SourceUrlRecorderWebContentsObserver;
class SourceUrlRecorderWebStateObserver;
}  // namespace internal

/**
 * This is UkmRecorder which forwards its calls to some number of other
 * UkmRecorders. This primarily provides a way for TestUkmRecorders to
 * receive copies of recorded metrics.
 */
class METRICS_EXPORT DelegatingUkmRecorder : public UkmRecorder {
 public:
  DelegatingUkmRecorder();

  DelegatingUkmRecorder(const DelegatingUkmRecorder&) = delete;
  DelegatingUkmRecorder& operator=(const DelegatingUkmRecorder&) = delete;

  ~DelegatingUkmRecorder() override;

  // Lazy global instance getter.
  static DelegatingUkmRecorder* Get();

  // Adds a recorder this one should send its calls to.
  // The caller is responsible for removing the delegate before it is destroyed.
  void AddDelegate(base::WeakPtr<UkmRecorder> delegate);

  // Removes a delegate added with AddDelegate.
  // The pointer is only used as a key.
  void RemoveDelegate(UkmRecorder* delegate);

  bool HasMultipleDelegates();

 private:
  friend class AppSourceUrlRecorder;
  friend class internal::SourceUrlRecorderWebContentsObserver;
  friend class internal::SourceUrlRecorderWebStateObserver;

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id,
                    const GURL& url,
                    const AppType app_type) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;
  void RecordWebDXFeatures(SourceId source_id,
                           const std::set<int32_t>& features,
                           const size_t max_feature_value) override;
  void MarkSourceForDeletion(SourceId source_id) override;

  class Delegate final {
   public:
    Delegate(scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::WeakPtr<UkmRecorder> ptr);
    Delegate(const Delegate& other);
    ~Delegate();

    void UpdateSourceURL(SourceId source_id, const GURL& url);
    void UpdateAppURL(SourceId source_id,
                      const GURL& url,
                      const AppType app_type);
    void RecordNavigation(SourceId source_id,
                          const UkmSource::NavigationData& navigation_data);
    void AddEntry(mojom::UkmEntryPtr entry);
    void RecordWebDXFeatures(SourceId source_id,
                             const std::set<int32_t>& features,
                             const size_t max_feature_value);
    void MarkSourceForDeletion(SourceId source_id);

   private:
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::WeakPtr<UkmRecorder> ptr_;
  };

  // Synchronizes access to |delegates_|.
  // Not using ObserverListThreadSafe since we need to make copies of call
  // arguments.
  mutable base::Lock lock_;

  std::unordered_map<UkmRecorder*, Delegate> delegates_;
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_DELEGATING_UKM_RECORDER_H_
