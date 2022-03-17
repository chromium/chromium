include_rules = [
    "+base/callback.h",
    "+base/callback_forward.h",
    "+base/callback_helpers.h",
    "+base/memory/raw_ptr.h",
    "+base/notreached.h",
    "+base/observer_list.h",
    "+base/trace_event",
    "+third_party/perfetto/include/perfetto/tracing",
]

specific_include_rules = {
    ".*_test\.cc": [
        # Some tests use base::RunLoop.
        "+base/run_loop.h",
    ],
}
