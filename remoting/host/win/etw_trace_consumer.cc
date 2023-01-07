// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/etw_trace_consumer.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/logging_win.h"
#include "base/threading/thread_checker.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/event_trace_controller.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/win/etw_trace_controller.h"
#include "remoting/host/win/event_trace_data.h"
#include "remoting/host/win/host_event_logger.h"

namespace remoting {

namespace {

class EtwTraceConsumerImpl : public EtwTraceConsumer {
 public:
  EtwTraceConsumerImpl();
  EtwTraceConsumerImpl(const EtwTraceConsumerImpl&) = delete;
  EtwTraceConsumerImpl& operator=(const EtwTraceConsumerImpl&) = delete;
  ~EtwTraceConsumerImpl() override;

  bool StartLogging(scoped_refptr<AutoThreadTaskRunner> task_runner,
                    std::vector<std::unique_ptr<HostEventLogger>> loggers);
  void StopLogging();

 private:
  class Core : public base::win::EtwTraceConsumerBase<Core> {
   public:
    Core();
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    ~Core();

    static VOID WINAPI ProcessEvent(EVENT_TRACE* event);

    bool Start(std::vector<std::unique_ptr<HostEventLogger>> loggers);
    void Stop();

    // Blocking call to begin receiving ETW events from Windows.  Must be called
    // on an IO thread which allows blocking.  Call Stop() to unblock the thread
    // and allow it to be cleaned up.
    void ConsumeEvents();

   private:
    // Parses an event and passes it along to the delegate for processing.
    void DispatchEvent(EVENT_TRACE* event);

    static Core* instance_;

    std::unique_ptr<EtwTraceController> controller_;

    std::vector<std::unique_ptr<HostEventLogger>> loggers_;

    THREAD_CHECKER(main_thread_checker_);
    THREAD_CHECKER(consume_thread_checker_);
  };

  std::unique_ptr<Core> core_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
};

// static
EtwTraceConsumerImpl::Core* EtwTraceConsumerImpl::Core::instance_ = nullptr;

// static
void EtwTraceConsumerImpl::Core::ProcessEvent(EVENT_TRACE* event) {
  // This method is called on the same thread as Consume().
  EtwTraceConsumerImpl::Core* instance = instance_;
  if (instance) {
    instance->DispatchEvent(event);
  }
}

EtwTraceConsumerImpl::Core::Core() {
  DETACH_FROM_THREAD(consume_thread_checker_);
}

EtwTraceConsumerImpl::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(consume_thread_checker_);
}

bool EtwTraceConsumerImpl::Core::Start(
    std::vector<std::unique_ptr<HostEventLogger>> loggers) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(!instance_);
  instance_ = this;

  controller_ = std::make_unique<EtwTraceController>();
  if (!controller_->Start()) {
    return false;
  }

  HRESULT hr = OpenRealtimeSession(controller_->GetActiveSessionName());
  if (FAILED(hr)) {
    return false;
  }

  loggers_ = std::move(loggers);

  return true;
}

void EtwTraceConsumerImpl::Core::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!instance_) {
    return;
  }

  DCHECK_EQ(instance_, this);
  if (controller_) {
    controller_->Stop();
    controller_.reset();
  }
  instance_ = nullptr;
}

void EtwTraceConsumerImpl::Core::ConsumeEvents() {
  // Consume will block the thread until the provider is disabled so make sure
  // it is not run on the same thread that |core_| was created on.
  DCHECK_CALLED_ON_VALID_THREAD(consume_thread_checker_);
  Consume();
}

void EtwTraceConsumerImpl::Core::DispatchEvent(EVENT_TRACE* event) {
  // This method is called on the same thread as Consume().
  DCHECK_CALLED_ON_VALID_THREAD(consume_thread_checker_);
  if (!event) {
    return;
  }

  if (!IsEqualGUID(event->Header.Guid, logging::kLogEventId)) {
    // Event was not logged from our provider.
    return;
  }

  EventTraceData data = EventTraceData::Create(event);
  if (data.event_type == logging::LOG_MESSAGE_FULL ||
      data.event_type == logging::LOG_MESSAGE) {
    for (const auto& logger : loggers_) {
      logger->LogEvent(data);
    }
  }
}

EtwTraceConsumerImpl::EtwTraceConsumerImpl() = default;

EtwTraceConsumerImpl::~EtwTraceConsumerImpl() {
  StopLogging();
}

bool EtwTraceConsumerImpl::StartLogging(
    scoped_refptr<AutoThreadTaskRunner> task_runner,
    std::vector<std::unique_ptr<HostEventLogger>> loggers) {
  DCHECK(!core_);

  core_ = std::make_unique<Core>();
  if (!core_->Start(std::move(loggers))) {
    core_.reset();
    return false;
  }

  task_runner_ = task_runner;
  // base::Unretained is safe because |core_| is destroyed on |task_runner_|.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EtwTraceConsumerImpl::Core::ConsumeEvents,
                                base::Unretained(core_.get())));
  return true;
}

void EtwTraceConsumerImpl::StopLogging() {
  if (!core_) {
    return;
  }

  // |core_| is consuming trace events on |task_runner_| which is effectively
  // blocked (Windows is calling it back but we can't schedule work on it).
  // To unblock that thread, we first need to stop tracing, after that we
  // schedule a deletion on the tracing thread so it occurs after all of the
  // pending events have been handled.
  core_->Stop();
  task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

}  // namespace

// static
std::unique_ptr<EtwTraceConsumer> EtwTraceConsumer::Create(
    scoped_refptr<AutoThreadTaskRunner> task_runner,
    std::vector<std::unique_ptr<HostEventLogger>> loggers) {
  DCHECK(!loggers.empty());

  auto etw_trace_consumer = std::make_unique<EtwTraceConsumerImpl>();
  if (!etw_trace_consumer->StartLogging(task_runner, std::move(loggers))) {
    LOG(ERROR) << "Failed to start EtwTraceConsumer instance.";
    return nullptr;
  }

  return etw_trace_consumer;
}

}  // namespace remoting
