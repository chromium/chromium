include_rules = [
  "+crypto",
  "+mojo/public",
  # For ipc_channel_nacl.cc:
  "+native_client/src/public",
  "+sandbox/mac/seatbelt.h",
  "+services/tracing/public/cpp",
  "+third_party/perfetto/protos/perfetto/trace/track_event",
]

specific_include_rules = {
  "ipc_(test|perftest)_(base|util)\.(cc|h)": [
    "+mojo/core/embedder",
    "+mojo/core/test",
  ],
  ".*(unit|perf)tests?\.cc": [
    "+mojo/core/embedder",
    "+mojo/core/test",
  ],
  "ipc_message_protobuf_utils\.h": [
    # Support serializing RepeatedField / RepeatedPtrField:
    "+third_party/protobuf/src/google/protobuf/repeated_field.h",
  ],
}
