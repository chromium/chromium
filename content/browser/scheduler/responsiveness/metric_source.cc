// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/metric_source.h"

#include "base/functional/bind.h"
#include "base/pending_task.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/message_loop_observer.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

MetricSource::Delegate::~Delegate() = default;

MetricSource::MetricSource(Delegate* delegate) : delegate_(delegate) {
  CHECK(delegate_, base::NotFatalUntil::M160);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);
}

void MetricSource::SetUp() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  RegisterMessageLoopObserverUI();
  native_event_observer_ui_ = CreateNativeEventObserver();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricSource::SetUpOnIOThread, base::Unretained(this)));
}

void MetricSource::Destroy(base::ScopedClosureRunner on_finish_destroy) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  CHECK(!destroy_was_called_, base::NotFatalUntil::M160);
  destroy_was_called_ = true;

  message_loop_observer_ui_.reset();
  native_event_observer_ui_.reset();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricSource::TearDownOnIOThread, base::Unretained(this),
                     std::move(on_finish_destroy)));
}

std::unique_ptr<BrowserUINativeEventObserver>
MetricSource::CreateNativeEventObserver() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  BrowserUINativeEventObserver::WillRunEventCallback will_run_callback =
      base::BindRepeating(&Delegate::WillRunEventOnUIThread,
                          base::Unretained(delegate_));
  BrowserUINativeEventObserver::DidRunEventCallback did_run_callback =
      base::BindRepeating(&Delegate::DidRunEventOnUIThread,
                          base::Unretained(delegate_));
  return std::make_unique<BrowserUINativeEventObserver>(
      std::move(will_run_callback), std::move(did_run_callback));
}

MetricSource::~MetricSource() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);
}

void MetricSource::RegisterMessageLoopObserverUI() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  auto will_run_callback = base::BindRepeating(&Delegate::WillRunTaskOnUIThread,
                                               base::Unretained(delegate_));
  auto did_run_callback = base::BindRepeating(&Delegate::DidRunTaskOnUIThread,
                                              base::Unretained(delegate_));
  message_loop_observer_ui_ = std::make_unique<MessageLoopObserver>(
      std::move(will_run_callback), std::move(did_run_callback));
}

void MetricSource::RegisterMessageLoopObserverIO() {
  // We can use base::Unretained(delegate_) since delegate_ is retained
  // in the constructor, and we won't release it when it is in use.
  auto will_run_callback = base::BindRepeating(&Delegate::WillRunTaskOnIOThread,
                                               base::Unretained(delegate_));
  auto did_run_callback = base::BindRepeating(&Delegate::DidRunTaskOnIOThread,
                                              base::Unretained(delegate_));
  message_loop_observer_io_ = std::make_unique<MessageLoopObserver>(
      std::move(will_run_callback), std::move(did_run_callback));
}

void MetricSource::SetUpOnIOThread() {
  CHECK_CURRENTLY_ON(content::BrowserThread::IO, base::NotFatalUntil::M160);

  RegisterMessageLoopObserverIO();

  delegate_->SetUpOnIOThread();
}

void MetricSource::TearDownOnIOThread(
    base::ScopedClosureRunner on_finish_destroy) {
  CHECK_CURRENTLY_ON(content::BrowserThread::IO, base::NotFatalUntil::M160);

  delegate_->TearDownOnIOThread();

  message_loop_observer_io_.reset();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricSource::TearDownOnUIThread, base::Unretained(this),
                     std::move(on_finish_destroy)));
}

void MetricSource::TearDownOnUIThread(
    base::ScopedClosureRunner on_finish_destroy) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  delegate_->TearDownOnUIThread();
  // |on_finish_destroy| isn't further passed on. It gets run here and might
  // destroy us.
}

}  // namespace responsiveness
}  // namespace content
