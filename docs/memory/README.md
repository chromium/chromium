# Memory

Landing page for all things memory-related in Chromium.

## How is chrome's memory usage doing in the world?

Look at the UMAs **Memory.{Total,Renderer,Browser,Gpu,Extension}.PrivateMemoryFootprint**.

## How do developers communicate?

Note, these channels are for developer coordination and NOT user support. If
you are a Chromium user experiencing a memory related problem, file a bug
instead.

| name | description |
|------|-------------|
| [memory-dev@chromium.org]() | Discussion group for all things memory related. Post docs, discuss bugs, etc., here. |
| chrome-memory@google.com | Google internal version of the above. Use sparingly. |
| https://chromium.slack.com/messages/memory/ | Slack channel for real-time discussion with memory devs. Lots of C++ sadness too. |
| crbug [Performance=Memory](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DMemory) label | Bucket with auto-filed and user-filed bugs. |
| crbug [Stability=Memory](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Stability%3DMemory) label | Tracks mostly OOM crashes. |

## I have memory problem, what do I do?

Follow [these instructions](/docs/memory/filing_memory_bugs.md) to file a high
quality bug.

## I'm a developer trying to investigate a memory issues, what do I do?

See [this page](/docs/memory/debugging_memory_issues.md) for further instructions.

## I'm a developer looking for more information. How do I get started?

Great! First, sign up for the mailing lists above and check out the slack channel.

Second, familiarize yourself with the following:

| Topic | Description |
|-------|-------------|
| [Key Concepts in Chrome Memory](/docs/memory/key_concepts.md) | Primer for memory terminology in Chrome. |
| [memory-infra](/docs/memory-infra/README.md) | The primary tool used for inspecting allocations. |

## What are people actively working on?

There are roughly three types of memory work within Chrome:

* Team based, targeted improvements. Examples include:
    * memory reductions for specific components [e.g. for v8]
    * allocator improvements [e.g. PartitionAlloc]
    * memory purging at appropriate times [e.g. on tab background]
    * better memory pressure signals
* Memlog: Heap profiling in the wild for regression detection + root cause
  analysis.
* Lab tests: Perf waterfall for micro-regressions, ASAN/MSAN/LSAN, blink leak
  detector.


## Key knowledge areas and contacts
| Knowledge Area | Contact points |
|----------------|----------------|
| Chrome on Android | lizeb, pasko, ssid |
| Browser Process | ssid, erikchen, etienneb |
| GPU/cc | ericrk |
| Memory metrics | ssid, erikchen, primano, ajwong, wez |
| Heap Profiling | alph, erikchen, ssid, etienneb |
| Net Stack | mmenke, rsleevi, xunjieli |
| Renderer Process | haraken, tasak, hajimehoshi, keishi, hiroshige |
| V8 | hpayer, ulan, verwaest, mlippautz |


## Other docs
  * [Why we work on memory](https://docs.google.com/document/d/1jhERqimO-LtuplzQzbBv1vK7SVOh63AMf2irJI2LOqU/edit)
  * [TOK/LON memory-dev@ meeting notes](https://docs.google.com/document/d/1tCTw9lnjs85t8GFiiyae2hbu6lrz8kysFCgMCKUvcXo/edit)
  * [Memory convergence 3 (in Mountain View, 2017 May)](https://docs.google.com/document/d/1FBIqBGIa0DSaFsh-QjmVvoC82pGuOgiQDIhc8-vzXbQ/edit)
  * [TRIM convergence 2 (in Mountain View, 2016 Nov)](https://docs.google.com/document/d/17Kef7UxjR6VW_ehVbsc-DI0IU7TQk-2C56JSbzbPuhA/edit)
  * [TRIM convergence 1 (in Munich, 2016 Apr)](https://docs.google.com/document/d/1PGcM6iVBp0OYh3m8xGQhOgkQK0obQy8YWwoefP9NZCA/edit#)

