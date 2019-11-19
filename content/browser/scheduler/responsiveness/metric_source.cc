// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/metric_source.h"

#include "base/bind.h"
#include "base/pending_task.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/message_loop_observer.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ui_base_features.h"
#endif

namespace content {
namespace responsiveness {

MetricSource::Delegate::~Delegate() = default;

MetricSource::MetricSource(Delegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MetricSource::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RegisterMessageLoopObserverUI();
  native_event_observer_ui_ = CreateNativeEventObserver();

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&MetricSource::SetUpOnIOThread, base::Unretained(this)));
}

void MetricSource::Destroy(base::ScopedClosureRunner on_finish_destroy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!destroy_was_called_);
  destroy_was_called_ = true;

  message_loop_observer_ui_.reset();
  native_event_observer_ui_.reset();

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&MetricSource::TearDownOnIOThread, base::Unretained(this),
                     std::move(on_finish_destroy)));
}

std::unique_ptr<NativeEventObserver> MetricSource::CreateNativeEventObserver() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  NativeEventObserver::WillRunEventCallback will_run_callback =
      base::BindRepeating(&Delegate::WillRunEventOnUIThread,
                          base::Unretained(delegate_));
  NativeEventObserver::DidRunEventCallback did_run_callback =
      base::BindRepeating(&Delegate::DidRunEventOnUIThread,
                          base::Unretained(delegate_));
  return std::make_unique<NativeEventObserver>(std::move(will_run_callback),
                                               std::move(did_run_callback));
}

MetricSource::~MetricSource() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MetricSource::RegisterMessageLoopObserverUI() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  MessageLoopObserver::TaskCallback will_run_callback = base::BindRepeating(
      &Delegate::WillRunTaskOnUIThread, base::Unretained(delegate_));
  MessageLoopObserver::TaskCallback did_run_callback = base::BindRepeating(
      &Delegate::DidRunTaskOnUIThread, base::Unretained(delegate_));
  message_loop_observer_ui_ = std::make_unique<MessageLoopObserver>(
      std::move(will_run_callback), std::move(did_run_callback));
}

void MetricSource::RegisterMessageLoopObserverIO() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  MessageLoopObserver::TaskCallback will_run_callback = base::BindRepeating(
      &Delegate::WillRunTaskOnIOThread, base::Unretained(delegate_));
  MessageLoopObserver::TaskCallback did_run_callback = base::BindRepeating(
      &Delegate::DidRunTaskOnIOThread, base::Unretained(delegate_));
  message_loop_observer_io_ = std::make_unique<MessageLoopObserver>(
      std::move(will_run_callback), std::move(did_run_callback));
}

void MetricSource::SetUpOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  RegisterMessageLoopObserverIO();

  delegate_->SetUpOnIOThread();
}

void MetricSource::TearDownOnIOThread(
    base::ScopedClosureRunner on_finish_destroy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  delegate_->TearDownOnIOThread();

  message_loop_observer_io_.reset();

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&MetricSource::TearDownOnUIThread, base::Unretained(this),
                     std::move(on_finish_destroy)));
}

void MetricSource::TearDownOnUIThread(
    base::ScopedClosureRunner on_finish_destroy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  delegate_->TearDownOnUIThread();
  // |on_finish_destroy| isn't further passed on. It gets run here and might
  // destroy us.
}

}  // namespace responsiveness
}  // namespace content
