// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "services/tracing/perfetto/track_event_json_exporter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"

// Tool to convert a given proto trace into json.
//
// Usage:
//   trace_json_exporter [input_file] [output_file]
// Both the arguments are required.
//
// Parses the given input file which contains a serialized perfetto::Trace
// proto, converts the trace to JSON and writes to output file.

namespace tracing {

void OnJsonData(base::File* output_file,
                std::string* json,
                base::DictionaryValue* metadata,
                bool has_more) {
  CHECK_EQ(output_file->WriteAtCurrentPos(json->data(), json->size()),
           static_cast<int>(json->size()));
}

void WriteJsonTrace(const std::string& data, base::File* output_file) {
  TrackEventJSONExporter exporter(
      JSONTraceExporter::ArgumentFilterPredicate(),
      JSONTraceExporter::MetadataFilterPredicate(),
      base::BindRepeating(&OnJsonData, base::Unretained(output_file)));
  perfetto::protos::pbzero::Trace::Decoder decoder(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  std::vector<perfetto::TracePacket> packets;
  for (auto it = decoder.packet(); !!it; ++it) {
    perfetto::TracePacket trace_packet;
    auto const_bytes = *it;
    trace_packet.AddSlice(const_bytes.data, const_bytes.size);
    packets.emplace_back(std::move(trace_packet));
  }
  exporter.OnTraceData(std::move(packets), false);
}

}  // namespace tracing

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() < 2u) {
    LOG(ERROR) << "Enter input and output path. Usage:"
                  "trace_json_exporter [input] [output]";
    return -1;
  }

  base::FilePath input_path(args[0]);
  base::FilePath output_path(args[1]);

  std::string contents;
  CHECK(base::ReadFileToString(input_path, &contents));

  base::File output_file(
      output_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  tracing::WriteJsonTrace(contents, &output_file);
  return 0;
}
