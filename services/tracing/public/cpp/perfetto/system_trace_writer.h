// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_TRACE_WRITER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_TRACE_WRITER_H_

#include <list>

#include "base/check.h"
#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace internal {
inline const std::string& GetString(const std::string& string) {
  return string;
}

inline const std::string& GetString(
    const scoped_refptr<base::RefCountedString>& string) {
  return string->as_string();
}
}  // namespace internal

// Writes system trace data (ftrace or JSON events) to the perfetto SMB. Makes
// sure to split up the data into small chunks to avoid exhausting the SMB with
// a large burst of data, as this would cause data loss.
template <typename StringType, typename DataSourceType = void>
class COMPONENT_EXPORT(TRACING_CPP) SystemTraceWriter {
 public:
  enum class TraceType { kFTrace, kJson };

  static constexpr size_t kMaxBatchSizeBytes = 1 * 1024 * 1024;  // 1 mB.

  SystemTraceWriter(uint32_t target_buffer, TraceType trace_type)
      : trace_type_(trace_type),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  SystemTraceWriter(const SystemTraceWriter&) = delete;
  SystemTraceWriter& operator=(const SystemTraceWriter&) = delete;

  void WriteData(const StringType& data) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    buffered_data_.push_back(data);
    if (!waiting_for_ack_)
      WriteNextBatch();
  }

  void Flush(base::OnceClosure on_flush_complete_callback) {
    if (!waiting_for_ack_) {
      task_runner_->PostTask(FROM_HERE, std::move(on_flush_complete_callback));
      return;
    }
    on_flush_complete_callback_ = std::move(on_flush_complete_callback);
  }

 private:
  using ChromeEventBundleHandle =
      protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

  void WriteNextBatch() {
    waiting_for_ack_ = false;
    if (buffered_data_.empty()) {
      if (on_flush_complete_callback_)
        std::move(on_flush_complete_callback_).Run();
      return;
    }

    while (current_batch_size_ < kMaxBatchSizeBytes &&
           !buffered_data_.empty()) {
      size_t data_size =
          std::min(kMaxBatchSizeBytes - current_batch_size_,
                   internal::GetString(buffered_data_.front()).size() -
                       current_data_pos_);
      const char* data_start =
          internal::GetString(buffered_data_.front()).data() +
          current_data_pos_;

      auto update_packet = [&](perfetto::TraceWriter::TracePacketHandle
                                   trace_packet_handle) {
        trace_packet_handle->set_timestamp(
            TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
        trace_packet_handle->set_timestamp_clock_id(
            base::tracing::kTraceClockId);
        ChromeEventBundleHandle event_bundle =
            ChromeEventBundleHandle(trace_packet_handle->set_chrome_events());

        switch (trace_type_) {
          case TraceType::kFTrace: {
            event_bundle->add_legacy_ftrace_output(data_start, data_size);
            break;
          }
          case TraceType::kJson: {
            auto* json_trace = event_bundle->add_legacy_json_trace();
            json_trace->set_type(
                perfetto::protos::pbzero::ChromeLegacyJsonTrace::USER_TRACE);
            json_trace->set_data(data_start, data_size);
            break;
          }
        }
      };

      DataSourceType::Trace([&](typename DataSourceType::TraceContext ctx) {
        update_packet(ctx.NewTracePacket());
      });
      current_batch_size_ += data_size;
      current_data_pos_ += data_size;
      if (current_data_pos_ >=
          internal::GetString(buffered_data_.front()).size()) {
        buffered_data_.pop_front();
        current_data_pos_ = 0;
      }
    }

    if (current_batch_size_ >= kMaxBatchSizeBytes || buffered_data_.empty()) {
      waiting_for_ack_ = true;
      current_batch_size_ = 0;
      auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
      auto task_runner = task_runner_;
      auto flush_callback = [weak_ptr, task_runner]() {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&SystemTraceWriter::WriteNextBatch, weak_ptr));
      };
      DataSourceType::Trace([&](typename DataSourceType::TraceContext ctx) {
        ctx.Flush(flush_callback);
      });
    }
  }

  TraceType trace_type_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::list<StringType> buffered_data_;
  size_t current_data_pos_ = 0;
  size_t current_batch_size_ = 0;
  bool waiting_for_ack_ = false;
  base::OnceClosure on_flush_complete_callback_;

  base::WeakPtrFactory<SystemTraceWriter> weak_ptr_factory_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_TRACE_WRITER_H_
