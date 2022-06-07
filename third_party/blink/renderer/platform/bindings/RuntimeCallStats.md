# Runtime Call Stats

## About

`RuntimeCallStats` is a group of counters used to track execution times and call counts of functions in Blink during JS Execution. V8 has its own corresponding implementation of runtime_call_stats, which is closely mirrored by Blink. Blink's implementation can be found in [runtime_call_stats.h](runtime_call_stats.h) and [runtime_call_stats.cpp](runtime_call_stats.cpp).

## Usage

Counters can be added by adding a name under one of the categories listed under FOR_EACH_COUNTER in runtime_call_stats.h and by using the RUNTIME_CALL_TIMER_SCOPE, RUNTIME_CALL_STATS_ENTER and RUNTIME_CALL_STATS_LEAVE macros. See documentation in [runtime_call_stats.h](runtime_call_stats.h) for more details.

Counters can also be directly added to the bindings layer in method and attribute callbacks by using the `[RuntimeCallStatsCounter]` IDL extended attribute (see [IDLExtendedAttributes.md](../../bindings/IDLExtendedAttributes.md#RuntimeCallStatsCounter_m_a) for more details).

## Viewing Results

Results can be seen through [chrome tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool). Run chrome with `--enable-blink-features=BlinkRuntimeCallStats`, and record a trace. Be sure to enable the `v8` and `v8.runtime_stats` (which is disabled by default) categories under 'Manually select settings'. After recording a trace, select the events recorded (for the website being analyzed) and click on 'Runtime call stats table'. The Blink runtime call stats should be visible below the V8 call stats table.

Alternatively, running chrome as follows `chrome --enable-blink-features=BlinkRuntimeCallStats --dump-blink-runtime-call-stats --single-process` will dump call stats to the logs when the browser is closed. Adding `--enable-logging=stderr` will display log output in stderr.

## RCS_COUNT_EVERYTHING

Adding `runtime_call_stats_count_everything = true` to a gn args file creates a special build where counters are added in the bindings layer to most Blink callbacks that are called by V8. This gives a more thorough breakdown of where time is spent executing Blink C++ during JS Execution.

It is disabled by default (and behind a compile time flag) as it adds a large number of counters (> 2000) which causes a significant increase in binary size. There is also a performance hit when RCS is enabled with this build due to the large number of counters and counters added to some very trivial getters and setters.
