# Trace Event Best Practices

This document outlines best practices for emitting trace events in Chrome,
focusing on clarity, performance, and effective debugging. It draws from the
current state of Chrome's tracing infrastructure and the [Perfetto
library](https://perfetto.dev/docs).

## Trace Category

Trace events are grouped into categories, which are essential for filtering and
organizing trace data. Proper categorization is crucial for effective trace
analysis.


### Built-in Categories
  * Refer to [`base/trace_event/builtin_categories.h`](https://cs.chromium.org/chromium/src/base/trace_event/builtin_categories.h) for a list of predefined
    categories.
  * Familiarize yourself with existing categories before creating new ones to
    avoid duplication.

### Naming Convention
  * Follow the `namespace.category(.sub_category)(.debug)` naming convention
  for new categories.
    * Example: `base.scheduling`, `renderer.compositor`.
  * Avoid generic categories like `toplevel`. These become "junk drawers" and
    are too noisy.
  * Use `.debug` suffix for debug categories instead of `TRACE_DISABLED_BY_DEFAULT()`.

### Category Descriptions
  * Document new categories with `.SetDescription()` to clarify their purpose.
  * Identify an owner (e.g., a team or individual) for each category in
    comments.
  * Add the `"debug"` tag for debug categories.

### Avoid Category Groups
  * Avoid emitting events to multiple categories. Category groups need to be
    defined for each combination that’s used in chrome, which can lead to
    combinatorial explosion.
  * Prefer leveraging tags to group a set of categories under a common tag
    instead.

## Trace Event Usage

### Trace Event Macros
  * Most use cases are served by `#include "base/trace_event/trace_event.h"`
  * **Use Perfetto Macros:** Employ the modern macros documented in
    [`perfetto/include/perfetto/tracing/track_event.h`](https://cs.chromium.org/chromium/src/third_party/perfetto/include/perfetto/tracing/track_event.h).
      *   `TRACE_EVENT()`
      *   `TRACE_EVENT_BEGIN()`
      *   `TRACE_EVENT_END()`
  * See
[`third_party/perfetto/docs/instrumentation/track-events.md`](https://cs.chromium.org/chromium/src/third_party/perfetto/docs/instrumentation/track-events.md) for more details.
  * **Avoid Legacy Macros:**  Do not use legacy macros defined in
    `third_party/perfetto/include/perfetto/tracing/track_event_legacy.h`, such
    as `TRACE_EVENT0/1/2`, `TRACE_EVENT_ASYNC_BEGIN0/1/2` or any other macro
    that has 0/1/2 suffix.
    * These macros are deprecated and should not be used in new code.
  * Do not emit synchronous events when a thread is idle. This yields misleading
    process activity summary shown by perfetto UI.

### Static Strings
  * **Always Use Static Strings:** For event names, *always* use static strings.
    * Dynamic strings are filtered out due to privacy concerns and appear as
      "filtered" in field traces.
    * Use
      [`perfetto::StaticString`](https://cs.chromium.org/chromium/src/third_party/perfetto/include/perfetto/tracing/string_helpers.h)
      to mark strings that are not known to be static at compile time but are in
      fact static.
   * If you need to add sensitive data in the trace event, prefer emitting it as
     arguments or proto fields, and keep the event name a static string.

## Asynchronous Events

Asynchronous events are emitted outside of the current thread's execution flow;
they are not bound to the thread where the `TRACE_EVENT_BEGIN` or
`TRACE_EVENT_END` macros are invoked. Use the `perfetto::Track` API from
[`third_party/perfetto/include/perfetto/tracing/track.h`](https://cs.chromium.org/chromium/src/third_party/perfetto/include/perfetto/tracing/track.h)
to specify which track they belong to, which can span across threads or even
processes. Asynchronous events are best suited for representing high-level,
long-lived states or operations that are conceptually independent of a single
thread's execution.

### Careful Usage
  * **Reserve for High-Level State:** Use asynchronous events sparingly,
    primarily for representing high-level, user-perceptible states or
    significant, long-lived operations.
    * Examples: First Contentful Paint (FCP), page visibility changes.
  * **Avoid for Debugging:** Avoid using async events for debugging object
    lifetimes or short-lived events. Async tracks take a lot of vertical space
    and nesting can be visually misleading. Alternatively
    * Put the event in a category with the `.debug` suffix (e.g., `mycomponent.debug`). This allows for easy filtering when collecting traces.
    * Use instant events with flows to retain threading context.

```cpp
TRACE_EVENT_INSTANT("my_component.debug.lifetime", "MyObject::Constructor",
    perfetto::Flow::FromPointer(this));

TRACE_EVENT_INSTANT("my_component.debug.lifetime", "MyObject::Destructor",
    perfetto::TerminatingFlow::FromPointer(this));
```

### Named Tracks
  * **Use Named Tracks:** Instead of emitting async events directly to a global
  track, emit them to a `NamedTrack`. To organize events logically, you can
  create a track hierarchy.

```cpp
#include "base/trace_event/track_event.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

perfetto::NamedTrack CreateFrameParentTrack() {
  return perfetto::NamedTrack("Frames", 0, perfetto::Track());
}

perfetto::NamedTrack GetFrameVisibleTrack(int64_t frame_id) {
  static const base::NoDestructor<
      base::trace_event::TrackRegistration<perfetto::NamedTrack>>
      parent_track(CreateFrameParentTrack());
  return perfetto::NamedTrack("Frame Visibility", frame_id,
      parent_track->track());
}

void MyFunction(int64_t frame_id, int64_t start_ts, int64_t end_ts) {
  TRACE_EVENT_BEGIN("renderer", "Visible",
      GetFrameVisibleTrack(frame_id), start_ts);
  TRACE_EVENT_END("renderer", "Visible",
      GetFrameVisibleTrack(frame_id), end_ts);
}
```

## 4. Testing

Use `base::test::TracingEnvironment` to enable tracing in unittests and `base::test::TestTraceProcessor` to control a tracing session and read events.
Avoid using legacy base::trace_event::TraceLog in new code.

```cpp
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/track_event.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MyComponentTest, TestTracing) {
  base::test::TracingEnvironment tracing_env;
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("my_category");

  // Code that emits trace events
  TRACE_EVENT("my_category", "MyScopedEvent");
  TRACE_EVENT_BEGIN("my_category", "MyScopedEvent");
  // ... some work ...
  TRACE_EVENT_END("my_category", "MyScopedEvent");
  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto result = ttp.RunQuery(R"sql(
    SELECT count(*) as cnt from slice where name = 'MyScopedEvent'
  )sql");
  ASSERT_TRUE(result.has_value()) << result.error();
  // verify that the expected events are present in result.
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"2"}));
}
```