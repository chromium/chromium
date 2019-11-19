// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
#define SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"

namespace perfetto {
namespace protos {
class ChromeLegacyJsonTrace;
class ChromeMetadata;
class ChromeTraceEvent_Arg;
class DebugAnnotation;
class TraceStats;
}  // namespace protos
}  // namespace perfetto

namespace tracing {

void OutputJSONFromArgumentProto(
    const perfetto::protos::ChromeTraceEvent_Arg& arg,
    std::string* out);
void OutputJSONFromArgumentProto(const perfetto::protos::DebugAnnotation& arg,
                                 std::string* out);

// Converts proto-encoded trace data into the legacy JSON trace format.
// Conversion happens on-the-fly as new trace packets are received.
class JSONTraceExporter {
 public:
  // Given argument name for the trace event, returns if the argument should be
  // filtered or not.
  using ArgumentNameFilterPredicate =
      base::RepeatingCallback<bool(const char* arg_name)>;

  // Given trace event name and category group name, returns a argument name
  // filter predicate callback that can filter arguments for the given event.
  using ArgumentFilterPredicate =
      base::RepeatingCallback<bool(const char* category_group_name,
                                   const char* event_name,
                                   ArgumentNameFilterPredicate*)>;

  // Given a metadata name, returns if the event should be filtered or not.
  using MetadataFilterPredicate =
      base::RepeatingCallback<bool(const std::string& metadata_name)>;

  using OnTraceEventJSONCallback = base::RepeatingCallback<
      void(std::string* json, base::DictionaryValue* metadata, bool has_more)>;

  JSONTraceExporter(ArgumentFilterPredicate argument_filter_predicate,
                    MetadataFilterPredicate metadata_filter_predicate,
                    OnTraceEventJSONCallback callback);
  virtual ~JSONTraceExporter();

  // Called to notify the exporter of new trace packets. Will call the
  // |json_callback| passed in the constructor with the converted trace data.
  void OnTraceData(std::vector<perfetto::TracePacket> packets, bool has_more);

  void SetArgumentFilterForTesting(ArgumentFilterPredicate predicate) {
    argument_filter_predicate_ = std::move(predicate);
  }

  void SetMetdataFilterPredicateForTesting(MetadataFilterPredicate predicate) {
    metadata_filter_predicate_ = std::move(predicate);
  }

  void set_label_filter(const std::string& label_filter) {
    label_filter_ = label_filter;
  }

 protected:
  class StringBuffer {
   public:
    StringBuffer(OnTraceEventJSONCallback callback);
    StringBuffer(const StringBuffer& copy) = delete;
    StringBuffer(StringBuffer&& move);
    ~StringBuffer();

    StringBuffer& operator+=(const std::string& input);

    StringBuffer& operator+=(std::string&& input);

    StringBuffer& operator+=(const char* input);

    std::string* mutable_out();
    const std::string& out();

    void reserve(size_t size);

    template <typename... Args>
    void AppendF(const char* format, Args&&... args) {
      MaybeRunCallback();
      base::StringAppendF(&out_, format, std::forward<Args>(args)...);
    }

    void EscapeJSONAndAppend(const std::string& unescaped);

    void Flush(base::DictionaryValue* metadata, bool has_more);

   private:
    // Depending on the size of the current output we might need to send a part
    // of it back.
    void MaybeRunCallback();

    std::string out_;
    OnTraceEventJSONCallback callback_;
  };

  class ArgumentBuilder {
   public:
    ArgumentBuilder(const ArgumentFilterPredicate& argument_filter_predicate,
                    const char* name,
                    const char* category_group_name,
                    StringBuffer* out);
    ~ArgumentBuilder();

    // Takes an arg name, and returns nullptr if
    //
    // a) all args are being stripped
    // b) if this arg name was stripped.
    //
    // If the StringBuffer pointer is valid then you should append a string that
    // is properly formatted json for this arg value.
    StringBuffer* MaybeAddArg(const std::string& name);

   private:
    StringBuffer* AddArg();
    bool ArgumentNameIsStripped(const std::string& name);
    bool SkipBecauseStripped(const std::string& name);

    StringBuffer* out_;
    bool strip_args_ = false;
    bool has_args_ = false;
    ArgumentNameFilterPredicate argument_name_filter_predicate_;
  };

  // Adds all required fields to |out| in proper JSON format. Only one
  // ScopedJSONTraceEventAppender should exist per |out| string at a time,
  // since the TraceEvent will not be finished until the
  // ScopedJSONTraceEventAppender goes out of scope.
  class ScopedJSONTraceEventAppender {
   public:
    // Only one reference should exist at a time. So moving is okay but copying
    // is disallowed.
    ScopedJSONTraceEventAppender(ScopedJSONTraceEventAppender&& move);
    ScopedJSONTraceEventAppender(const ScopedJSONTraceEventAppender& copy) =
        delete;

    // Ensures that the JSON object is properly closed.
    ~ScopedJSONTraceEventAppender();

    // Optional traceEvent fields can also be set with the methods below. All
    // methods should only be called once.

    void AddDuration(int64_t duration);
    void AddThreadDuration(int64_t thread_duration);
    void AddThreadTimestamp(int64_t thread_timestamp);
    void AddThreadInstructionCount(int64_t thread_instruction_count);
    void AddThreadInstructionDelta(int64_t thread_instruction_delta);
    void AddBindId(uint64_t bind_id);
    // A set of bit flags for this trace event, along with a |scope|. |scope| is
    // ignored if empty.
    void AddFlags(uint32_t flags,
                  base::Optional<uint64_t> id,
                  const std::string& scope);

    // Begins constructing the args sections, and finishes when ArgumentBuilder
    // is destroyed. No other Add* function should be called until
    // ArgumentBuilder goes out of scope.
    //
    // This should be used as follows.
    // {
    //   auto arg_builder = scoped_appender.BuildArgs();
    //   for (const auto& arg : args) {
    //     JSONTraceExporter::StringBuffer* maybe_arg =
    //         arg_builder->MaybeAddArg(arg.name);
    //     if (maybe_arg) {
    //       // Then one of the following to add the value in |arg|.
    //       *maybe_arg += "\"json_formatted_raw_value\"";
    //       maybe_arg->AppendF("\"%d\"", arg.integer);
    //       maybe_arg->EscapeJSONAndAppend("json_that will be : escaped");
    //     }
    //   }
    // }
    //
    // IMPORTANT: ArgumentBuilder must be deconstructed before the
    // ScopedJSONTraceEventAppender that created it is.
    std::unique_ptr<ArgumentBuilder> BuildArgs();

   private:
    // Subclasses of JSONTraceExporter can create a new instance by calling
    // AddTraceEvent().
    ScopedJSONTraceEventAppender(
        StringBuffer* out,
        ArgumentFilterPredicate argument_filter_predicate,
        const char* name,
        const char* categories,
        int32_t phase,
        int64_t timestamp,
        int32_t pid,
        int32_t tid);
    friend class JSONTraceExporter;

    char phase_;
    bool added_args_;
    StringBuffer* out_;
    const char* event_name_;
    const char* category_group_name_;
    ArgumentFilterPredicate argument_filter_predicate_;
  };

  // Subclasses implement this to add data from |packets| to the JSON output.
  // For example they can add traceEvents through AddTraceEvent(), or add
  // metadata through AddChromeMetadata().
  virtual void ProcessPackets(const std::vector<perfetto::TracePacket>& packets,
                              bool has_more) = 0;

  // If true then all trace events should be skipped. AddTraceEvent should not
  // be called.
  bool ShouldOutputTraceEvents() const;

  // Used for passing legacy JSON traces. This will update either the
  // traceEvents directly if needed by calling AddJSONTraceEvent or will store
  // the system trace information to be appended after the packets have been
  // processed.
  void AddChromeLegacyJSONTrace(
      const perfetto::protos::ChromeLegacyJsonTrace& json_trace);
  // Adds system Ftrace data to be appended to the trace JSON after all the
  // traceEvents have been processed.
  void AddLegacyFtrace(const std::string& legacy_ftrace_output);
  // Used to append ChromeMetadata to the trace. Can be called at any point.
  // Metadata is always appended after all packets have been processed.
  void AddChromeMetadata(const perfetto::protos::ChromeMetadata& metadata);
  // Writes (overwriting if already set) the perfetto trace stats to the
  // metadata that will be appended after all packets have been processed.
  void SetTraceStatsMetadata(const perfetto::protos::TraceStats& stats);

  // Used when sub-classes are adding a new trace event to the traceEvents
  // array. This will ensure that only proper json is appended.
  ScopedJSONTraceEventAppender AddTraceEvent(const char* name,
                                             const char* categories,
                                             int32_t phase,
                                             int64_t timestamp,
                                             int32_t pid,
                                             int32_t tid);

  void AddMetadata(const std::string& entry_name,
                   std::unique_ptr<base::Value> value);

 private:
  // Used by the implementation to ensure the proper separators exist between
  // trace events in the array.
  StringBuffer* AddJSONTraceEvent();

  StringBuffer out_;
  bool has_output_first_event_ = false;
  bool has_output_json_preamble_ = false;
  std::string legacy_json_trace_events_;
  std::string label_filter_;
  std::string legacy_system_ftrace_output_;
  std::unique_ptr<base::DictionaryValue> metadata_;
  ArgumentFilterPredicate argument_filter_predicate_;
  MetadataFilterPredicate metadata_filter_predicate_;

  DISALLOW_COPY_AND_ASSIGN(JSONTraceExporter);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
