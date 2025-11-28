// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"

namespace content {

class PipScreenCaptureCoordinatorImpl;

class PipScreenCaptureCoordinatorProxyImpl
    : public PipScreenCaptureCoordinatorProxy {
 public:
  PipScreenCaptureCoordinatorProxyImpl(
      base::WeakPtr<PipScreenCaptureCoordinatorImpl> coordinator,
      std::optional<NativeWindowId> initial_pip_window_id,
      const std::vector<CaptureInfo>& initial_captures);

  ~PipScreenCaptureCoordinatorProxyImpl() override;

  std::optional<NativeWindowId> PipWindowId() const override;
  const std::vector<CaptureInfo>& Captures() const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  class UiThreadObserver;
  friend class UiThreadObserver;

  void SetPipWindowId(const std::optional<NativeWindowId>& new_pip_window_id);
  void SetCaptures(const std::vector<CaptureInfo>& captures);

  base::WeakPtr<PipScreenCaptureCoordinatorImpl> coordinator_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<NativeWindowId> pip_window_id_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<CaptureInfo> captures_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> bound_sequence_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<PipScreenCaptureCoordinatorProxy::Observer> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<UiThreadObserver, base::OnTaskRunnerDeleter>
      ui_thread_observer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipScreenCaptureCoordinatorProxyImpl> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_PROXY_IMPL_H_
