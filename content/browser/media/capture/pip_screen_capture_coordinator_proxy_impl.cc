// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy_impl.h"

#include "base/functional/bind.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class PipScreenCaptureCoordinatorProxyImpl::UiThreadObserver
    : public PipScreenCaptureCoordinatorImpl::Observer {
 public:
  UiThreadObserver(base::WeakPtr<PipScreenCaptureCoordinatorProxyImpl> proxy,
                   scoped_refptr<base::SequencedTaskRunner> proxy_task_runner)
      : proxy_(proxy), proxy_task_runner_(std::move(proxy_task_runner)) {
    // Constructor runs on proxy's sequence.
    DETACH_FROM_SEQUENCE(ui_thread_sequence_checker_);
  }

  ~UiThreadObserver() override {
    // Destructor runs on UI thread.
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_thread_sequence_checker_);
    if (coordinator_) {
      coordinator_->RemoveObserver(this);
    }
  }

  void StartObservingOnUIThread(
      base::WeakPtr<PipScreenCaptureCoordinatorImpl> coordinator) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_thread_sequence_checker_);
    coordinator_ = coordinator;
    if (coordinator_) {
      coordinator_->AddObserver(this);

      // Update the proxy with the latest state
      OnPipWindowIdChanged(coordinator_->PipWindowId());
      OnCapturesChanged(coordinator_->Captures());
    }
  }

  // PipScreenCaptureCoordinatorImpl::Observer:
  void OnPipWindowIdChanged(
      std::optional<NativeWindowId> new_pip_window_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_thread_sequence_checker_);
    proxy_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipScreenCaptureCoordinatorProxyImpl::SetPipWindowId,
                       proxy_, new_pip_window_id));
  }

  void OnCapturesChanged(
      const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&
          captures) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_thread_sequence_checker_);
    proxy_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipScreenCaptureCoordinatorProxyImpl::SetCaptures,
                       proxy_, captures));
  }

 private:
  base::WeakPtr<PipScreenCaptureCoordinatorImpl> coordinator_;
  base::WeakPtr<PipScreenCaptureCoordinatorProxyImpl> proxy_;
  scoped_refptr<base::SequencedTaskRunner> proxy_task_runner_;
  SEQUENCE_CHECKER(ui_thread_sequence_checker_);
};

PipScreenCaptureCoordinatorProxyImpl::PipScreenCaptureCoordinatorProxyImpl(
    base::WeakPtr<PipScreenCaptureCoordinatorImpl> coordinator,
    std::optional<NativeWindowId> initial_pip_window_id,
    const std::vector<CaptureInfo>& initial_captures)
    : coordinator_(std::move(coordinator)),
      pip_window_id_(initial_pip_window_id),
      captures_(initial_captures),
      ui_thread_observer_(
          nullptr,
          base::OnTaskRunnerDeleter(GetUIThreadTaskRunner({}))) {
  // The proxy is created on one sequence (e.g., UI thread) but will be
  // used and destroyed on another (e.g., device thread).
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PipScreenCaptureCoordinatorProxyImpl::~PipScreenCaptureCoordinatorProxyImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `ui_thread_observer_` is automatically deleted on the UI thread via
  // `OnTaskRunnerDeleter`, which will unregister the observer.
}

std::optional<NativeWindowId>
PipScreenCaptureCoordinatorProxyImpl::PipWindowId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pip_window_id_;
}

const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&
PipScreenCaptureCoordinatorProxyImpl::Captures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return captures_;
}

void PipScreenCaptureCoordinatorProxyImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!bound_sequence_task_runner_) {
    bound_sequence_task_runner_ =
        base::SequencedTaskRunner::GetCurrentDefault();
  }

  observers_.AddObserver(observer);

  if (ui_thread_observer_ || !coordinator_) {
    return;
  }

  auto ui_observer = std::make_unique<UiThreadObserver>(
      weak_factory_.GetWeakPtr(), bound_sequence_task_runner_);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UiThreadObserver::StartObservingOnUIThread,
                     base::Unretained(ui_observer.get()), coordinator_));

  ui_thread_observer_ =
      std::unique_ptr<UiThreadObserver, base::OnTaskRunnerDeleter>(
          ui_observer.release(),
          base::OnTaskRunnerDeleter(GetUIThreadTaskRunner({})));
}

void PipScreenCaptureCoordinatorProxyImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  if (observers_.empty() && ui_thread_observer_) {
    ui_thread_observer_.reset();
  }
}

void PipScreenCaptureCoordinatorProxyImpl::SetPipWindowId(
    const std::optional<NativeWindowId>& new_pip_window_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pip_window_id_ == new_pip_window_id) {
    return;
  }
  pip_window_id_ = new_pip_window_id;
  for (Observer& obs : observers_) {
    obs.OnPipWindowIdChanged(pip_window_id_);
  }
}

void PipScreenCaptureCoordinatorProxyImpl::SetCaptures(
    const std::vector<CaptureInfo>& captures) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (captures_ == captures) {
    return;
  }
  captures_ = captures;
  for (Observer& obs : observers_) {
    obs.OnCapturesChanged(captures_);
  }
}

}  // namespace content
