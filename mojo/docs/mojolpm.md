# Getting started with MojoLPM

***
**Note:** Using MojoLPM to fuzz your Mojo interfaces is intended to be simple,
but there are edge-cases that may require a very detailed understanding of the
Mojo implementation to fix. If you run into problems that you can't understand
readily, send an email to [markbrand@google.com] and cc `fuzzing@chromium.org`
and we'll try and help.

**Prerequisites:** Knowledge of [libfuzzer] and basic understanding
of [Protocol Buffers] and [libprotobuf-mutator]. Basic understanding of
[testing in Chromium].
***

This document will walk you through:
* An overview of MojoLPM and what it's used for.
* Adding a fuzzer to an existing Mojo interface using MojoLPM.

[TOC]

## Overview of MojoLPM

MojoLPM is a toolchain for automatically generating structure-aware fuzzers for
Mojo interfaces using libprotobuf-mutator as the fuzzing engine.

This tool works by using the existing "grammar" for the interface provided by
the .mojom files, and translating that into a Protocol Buffer format that can be
fuzzed by libprotobuf-mutator. These protocol buffers are then interpreted by
a generated runtime as a sequence of mojo method calls on the targeted
interface.

The intention is that using these should be as simple as plugging the generated
code in to the existing unittests for those interfaces - so if you've already
implemented the necessary mocks to unittest your code, the majority of the work
needed to get quite effective fuzzing of your interfaces is already complete!

## Choose the Mojo interface(s) to fuzz

If you're a developer looking to add fuzzing support for an interface that
you're developing, then this should be very easy for you!

If not, then a good starting point is to search for [interfaces] in codesearch.
The most interesting interfaces from a security perspective are those which are
implemented in the browser process and exposed to the renderer process, but
there isn't a very simple way to enumerate these, so you may need to look
through some of the source code to find an interesting one.

A few of the places which bind many of these cross-privilege interfaces are
`content/browser/browser_interface_binders.cc` and
`content/browser/render_process_host_impl.cc`, specifically `RenderProcessHostImpl::RegisterMojoInterfaces`.

For the rest of this guide, we'll write a new fuzzer for
`blink.mojom.CodeCacheHost`, which is defined in
`third_party/blink/public/mojom/loader/code_cache.mojom`.

We then need to find the relevant GN build target for this mojo interface so
that we know how to refer to it later - in this case that is
`//third_party/blink/public/mojom:mojom_platform`.

## Find the implementations of the interfaces

If you are developing these interfaces, then you already know where to find the
implementations.

Otherwise a good starting point is to search for references to
"public blink::mojom::CodeCacheHost". Usually there is only a single
implementation of a given Mojo interface (there are a few exceptions where the
interface abstracts platform specific details, but this is less common). This
leads us to `content/browser/renderer_host/code_cache_host_impl.h` and
`CodeCacheHostImpl`.

## Find the unittest for the implementation

Specifically, we're looking for a browser-process side unittest (so not in
`//third_party/blink`). We want the unittest for the browser side implementation
of that Mojo interface - in many cases if such exists, it will be directly next
to the implementation source, ie. in this case we would be most likely to find
them in `content/browser/renderer_host/code_cache_host_impl_unittest.cc`.

Unfortunately, it doesn't look like `CodeCacheHostImpl` has a unittest, so we'll
have to go through the process of understanding how to create a valid instance
ourselves in order to fuzz this interface.

Since this implementation runs in the Browser process, and is part of `/content`,
we're going to create our new fuzzer in `/content/test/fuzzer`.

## Add our testcase proto

First we'll add a proto source file, `code_cache_host_mojolpm_fuzzer.proto`,
which is going to define the structure of our testcases. This is basically
boilerplate, but it allows creating fuzzers which interact with multiple Mojo
interfaces to uncover more complex issues. For our case, this will be a simple
file:

Note that the structure used here is shared between all MojoLPM fuzzers, and
while it is possible to come up with your own testcase format it would be
preferred if you use this same structure and simply add the appropriate Actions
for your fuzzer. This will allow more code-reuse between fuzzers, and also
allow corpus-merging between related fuzzers.

```
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message format for the MojoLPM fuzzer for the CodeCacheHost interface.

syntax = "proto2";

package content.fuzzing.code_cache_host.proto;

import "third_party/blink/public/mojom/loader/code_cache.mojom.mojolpm.proto";

// Bind a new CodeCacheHost remote
message NewCodeCacheHostAction {
  required uint32 id = 1;
}

// Run the specific sequence for (an indeterminate) period. This is not
// intended to create a specific ordering, but to allow the fuzzer to delay a
// later task until previous tasks have completed.
message RunThreadAction {
  enum ThreadId {
    IO = 0;
    UI = 1;
  }

  required ThreadId id = 1;
}

// Actions that can be performed by the fuzzer.
message Action {
  oneof action {
    NewCodeCacheHostAction new_code_cache_host = 1;
    RunThreadAction run_thread = 2;
    mojolpm.blink.mojom.CodeCacheHost.RemoteAction
        code_cache_host_remote_action = 3;
  }
}

// Sequence provides a level of indirection which allows Testcase to compactly
// express repeated sequences of actions.
message Sequence {
  repeated uint32 action_indexes = 1 [packed = true];
}

// Testcase is the top-level message type interpreted by the fuzzer.
message Testcase {
  repeated Action actions = 1;
  repeated Sequence sequences = 2;
  repeated uint32 sequence_indexes = 3 [packed = true];
}
```

This specifies all of the actions that the fuzzer will be able to take - it
will be able to create a new `CodeCacheHost` instance, perform sequences of
interface calls on those instances, and wait for various threads to be idle.

In order to build this proto file, we'll need to copy it into the out/ directory
so that it can reference the proto files generated by MojoLPM - this will be
handled for us by the `mojolpm_fuzzer_test` build rule.

## Add our fuzzer source

Now we're ready to create the fuzzer c++ source file,
`code_cache_host_mojolpm_fuzzer.cc` and the fuzzer build target. This
target is going to depend on both our proto file, and on the c++ source file.
Most of the necessary dependencies will be handled for us, but we do still need
to add some directly.

Note especially the dependency on `mojom_platform_mojolpm` in blink, this is an
autogenerated target where the target containing the generated fuzzer protocol
buffer descriptions will be the name of the mojom target with `_mojolpm`
appended. You'll need to make sure that your `mojolpm_fuzzer_test` target has
the correct dependencies here for all of the needed .mojolpm.proto imports.

(A good way to find these dependencies is to search in codesearch for
[`"code_cache_host.mojom f:.gn$"`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/BUILD.gn?q=code_cache.mojom%20f:gn&ss=chromium) to find the target that builds that mojom file.)

In addition, in `content/test/fuzzer/mojolpm_fuzzer_support.h` there is some
common code used to share the basics of a browser-process like environment
between fuzzers. New fuzzers in other areas of the codebase may need to build
something similar.

```
mojolpm_fuzzer_test("code_cache_host_mojolpm_fuzzer") {
  sources = [
    "code_cache_host_mojolpm_fuzzer.cc"
  ]

  proto_source = "code_cache_host_mojolpm_fuzzer.proto"

  deps = [
    ":mojolpm_fuzzer_support",
    "//content/browser:for_content_tests",
    "//content/public/browser:browser_sources",
  ]

  proto_deps = [
    "//third_party/blink/public/mojom:mojom_platform_mojolpm",
  ]
}
```

Now, the minimal source code to load our testcases:

```c++
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "code_cache_host_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::code_cache_host::proto::Testcase& testcase) {
}
```

You should now be able to build and run this fuzzer (it, of course, won't do
very much) to check that everything is lined up right so far. Recommended GN
arguments:

```
# DCHECKS are really useful when getting your fuzzer up and running correctly,
# but will often get in the way when running actual fuzzing, so we will disable
# this later.
dcheck_always_on = true

# Without this flag, our fuzzer target won't exist.
enable_mojom_fuzzer = true

# ASAN is super useful for fuzzing, but in this case we just want it to help us
# debug the inevitable lifetime issues while we get everything set-up correctly!
is_asan = true
is_component_build = true
is_debug = false
optimize_for_fuzzing = true
use_remoteexec = true
use_libfuzzer = true
```


## Handle global process setup

Now we need to add some basic setup code so that our process has something that
mostly resembles a normal Browser process; if you look in the file this is
`FuzzerEnvironmentWithTaskEnvironment`, which adds a global environment instance
that will handle setting up this basic environment, which will be reused for all
of our testcases, since starting threads is expensive and slow. This code should
also be responsible for setting up any "stateless" (or more-or-less stateless)
code that is required for your interface to run - examples are initializing the
Mojo Core, and loading ICU datafiles.

A key difference between our needs here and those of a normal unittest is that
we very likely do not want to be running in a special single-threaded mode. We
want to be able to trigger issues related to threading, sequencing and ordering,
and making sure that the UI, IO and threadpool threads behave as close to a
normal browser process as possible is desirable.

It's likely better to be conservative here - while it might appear that an
interface to be tested has no interaction with the UI thread, and so we could
save some resources by only having a real IO thread, it's often very difficult
to establish this with certainty.

In practice, the most efficient way forward will be to copy the existing
`Environment` setup from another MojoLPM fuzzer and adapting that to the
context in which the interface to be fuzzed will actually run. Most fuzzers in
content will be fine using either the existing `FuzzerEnvironment` or
`FuzzerEnvironmentWithTaskEnvironment`, depending on whether there's some
per-testcase state that causes issues with reusing the task environment. There
are existing examples of both in //content/test/fuzzer.


## Handle per-testcase setup

We next need to handle the necessary setup to instantiate `CodeCacheHostImpl`,
so that we can actually run the testcases. At this point, we realise that it's
likely that we want to be able to have multiple `CodeCacheHostImpl`'s with
different render_process_ids and different backing origins, so we need to modify
our proto file to reflect this:

```
message NewCodeCacheHost {
  enum OriginId {
    ORIGIN_A = 0;
    ORIGIN_B = 1;
    ORIGIN_OPAQUE = 2;
    ORIGIN_EMPTY = 3;
  }

  required uint32 id = 1;
  required uint32 render_process_id = 2;
  required OriginId origin_id = 3;
}
```

Note that we're using an enum to represent the origin, rather than a string;
it's unlikely that the true value of the origin is going to be important, so
we've instead chosen a few select values based on the cases mentioned in the
source.

The next thing that we need to do is to figure out the basic setup needed to
instantiate the interface we're interested in. Looking at the constructor for
`CodeCacheHostImpl` we need three things; a valid `render_process_id`, an
instance of `CacheStorageContextImpl` and an instance of
`GeneratedCodeCacheContext`. `CodeCacheHostFuzzerContext` is our container for
these per-testcase instances; and will handle creating and binding the instances
of the Mojo interfaces that we're going to fuzz.

The most important thing to be careful of here is that everything happens on
the correct thread/sequence. Many Browser-process objects have specific
expectations, and will end up with very different behaviour if they are created
or used from the wrong context.

See [here](https://bugs.chromium.org/p/chromium/issues/detail?id=1275431) for
an example of a false-positive crash caused by a change in sequencing behaviour
that was not immediately mirrored by the fuzzer.

If your test case requires the existence of a `RenderFrameHost` and similar
structures, see `content/test/fuzzer/presentation_service_mojolpm_fuzzer.cc`
for a fuzzer which already sets them up (in particular, using
`RenderViewHostTestHarnessAdapter`).

**The most important thing to be careful of here is that everything happens on
the correct thread/sequence. Many Browser-process objects have specific
expectations, and will end up with very different behaviour if they are created
or used from the wrong context. Test code doesn't always behave the same way, so
try to check the behaviour in the real Browser.**

**The second most important thing to be aware of is to make sure that the fuzzer
has the same control over lifetimes of objects that a renderer process would
normally have - the best way to check this is to make sure that you've found and
understood the browser process code that would usually bind that interface.**

## Integrate with the generated MojoLPM fuzzer code

Finally, we need to do a little bit more plumbing, to rig up this infrastructure
that we've built together with the autogenerated code that MojoLPM gives us to
interpret and run our testcases.

We need to implement the `CodeCacheHostTestcase`, and by inheriting from
`mojolpm::Testcase` we'll automatically get handling of the testcase format; we
just need to implement code to run at the start and end of each testcase, and
to run each individual action.

All three of these functions will be called on the Fuzzer thread; they should
ensure that after they've completed the `done_closure/run_closure` argument is
invoked on the Fuzzer thread.

```c++
void CodeCacheHostTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CodeCacheHostTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CodeCacheHostTestcase::RunAction(const ProtoAction& action,
                                      base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_IO;

  switch (action.action_case()) {
    case ProtoAction::kNewCodeCacheHost:
      AddCodeCacheHost(action.new_code_cache_host().id(),
                       action.new_code_cache_host().render_process_id(),
                       action.new_code_cache_host().origin_id(),
                       std::move(run_closure));
      return;

    case ProtoAction::kRunThread:
      // These actions ensure that any tasks currently queued on the named
      // thread have chance to run before the fuzzer continues.
      //
      // We don't provide any particular guarantees here; this does not mean
      // that the named thread is idle, nor does it prevent any other threads
      // from running (or the consequences of any resulting callbacks, for
      // example).
      if (action.run_thread().id() == ThreadId_UI) {
        content::GetUIThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      } else if (action.run_thread().id() == ThreadId_IO) {
        content::GetIOThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      }
      return;

    case ProtoAction::kCodeCacheHostRemoteAction:
      mojolpm::HandleRemoteAction(action.code_cache_host_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}
```

The key line here in integration with MojoLPM is the last case,
`kCodeCacheHostCall`, where we're asking MojoLPM to treat this incoming proto
entry as a call to a method on the `CodeCacheHost` interface.

There's just a little bit more boilerplate in the bottom of the file to tidy up
concurrency loose ends, making sure that the fuzzer components are all running
on the correct threads; those are more-or-less common to any fuzzer using
MojoLPM.


## Resulting structure

Overall, the structure of your fuzzer is likely approximately to reflect that
of the `content/test/fuzzer/presentation_service_mojolpm_fuzzer.cc`,
shown here:

![alt text](mojolpm-fuzzer-structure.png "Architecture diagram showing
the rough structure of the presentation service fuzzer")

(drawing source
[here](https://goto.google.com/mojolpm-fuzzer-structure) )



## Test it!

Make a corpus directory and fire up your shiny new fuzzer!

```
 ~/chromium/src% set ASAN_OPTIONS=detect_odr_violation=0,handle_abort=1,handle_sigtrap=1,handle_sigill=1
 ~/chromium/src% out/Default/code_cache_host_mojolpm_fuzzer /dev/shm/corpus
INFO: Seed: 3273881842
INFO: Loaded 1 modules   (1121912 inline 8-bit counters): 1121912 [0x559151a1aea8, 0x559151b2cd20),
INFO: Loaded 1 PC tables (1121912 PCs): 1121912 [0x559151b2cd20,0x559152c4b4a0),
INFO:      146 files found in /dev/shm/corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 146 min: 2b max: 268b total: 8548b rss: 88Mb
#147  INITED cov: 4633 ft: 10500 corp: 138/8041b exec/s: 0 rss: 91Mb
#152  NEW    cov: 4633 ft: 10501 corp: 139/8139b lim: 4096 exec/s: 0 rss: 91Mb L: 98/268 MS: 8 Custom-ChangeByte-Custom-EraseBytes-Custom-ShuffleBytes-Custom-Custom-
#154  NEW    cov: 4634 ft: 10510 corp: 140/8262b lim: 4096 exec/s: 0 rss: 91Mb L: 123/268 MS: 3 CustomCrossOver-ChangeBit-Custom-
#157  NEW    cov: 4634 ft: 10512 corp: 141/8384b lim: 4096 exec/s: 0 rss: 91Mb L: 122/268 MS: 3 CustomCrossOver-Custom-CustomCrossOver-
#158  NEW    cov: 4634 ft: 10514 corp: 142/8498b lim: 4096 exec/s: 0 rss: 91Mb L: 114/268 MS: 1 CustomCrossOver-
#159  NEW    cov: 4634 ft: 10517 corp: 143/8601b lim: 4096 exec/s: 0 rss: 91Mb L: 103/268 MS: 1 Custom-
#160  NEW    cov: 4634 ft: 10526 corp: 144/8633b lim: 4096 exec/s: 0 rss: 91Mb L: 32/268 MS: 1 Custom-
#164  NEW    cov: 4634 ft: 10528 corp: 145/8851b lim: 4096 exec/s: 0 rss: 91Mb L: 218/268 MS: 4 CustomCrossOver-Custom-CustomCrossOver-Custom-
```

## Wait for it...

Let the fuzzer run for a while, and keep periodically checking in in case it's
fallen over. It's likely you'll have made a few mistakes somewhere along the way
but hopefully soon you'll have the fuzzer running 'clean' for a few hours.

If you run into DCHECK failures in deserialization, see the section below marked
[triage].

## Expand it to include all relevant interfaces

`CodeCacheHost` is a very simple interface, and it doesn't have any dependencies
on other interfaces. In reality, most Mojo interfaces are much more complex, and
fuzzing their implementations thoroughly will require more work. We'll take a
quick look at a more complex interface, the `BlobRegistry`. If we look at
`blob_registry.mojom`:

```
// This interface is the primary access point from renderer to the browser's
// blob system. This interface provides methods to register new blobs and get
// references to existing blobs.
interface BlobRegistry {
  // Registers a new blob with the blob registry.
  // TODO(mek): Make this method non-sync and get rid of the UUID parameter once
  // enough of the rest of the system doesn't rely on the UUID anymore.
  [Sync] Register(pending_receiver<blink.mojom.Blob> blob, string uuid,
                  string content_type, string content_disposition,
                  array<DataElement> elements) => ();

  // Creates a new blob out of a data pipe.
  // |length_hint| is only used as a hint, to decide if the blob should be
  // stored in memory or on disk. Registration will still succeed even if less
  // or more bytes are read from the pipe. The resulting SerializedBlob can be
  // inspected to see how many bytes actually did end up being read from
  // the pipe. Pass 0 if nothing is known about the expected size.
  // If something goes wrong (for example the blob system doesn't have enough
  // available space to store all the data from the stream) null will be
  // returned.
  RegisterFromStream(string content_type, string content_disposition,
                     uint64 length_hint,
                     handle<data_pipe_consumer> data,
                     pending_associated_remote<ProgressClient>? progress_client)
      => (SerializedBlob? blob);

  // Returns a reference to an existing blob. Should not be used by new code,
  // is only exposed to make converting existing blob using code easier.
  // TODO(mek): Remove when crbug.com/740744 is resolved.
  [Sync] GetBlobFromUUID(pending_receiver<Blob> blob, string uuid) => ();

  // Returns a BlobURLStore for a specific origin.
  URLStoreForOrigin(url.mojom.Origin origin,
                    pending_associated_receiver<blink.mojom.BlobURLStore> url_store);
};
```

We can see that this interface references multiple other interfaces; there are
several different kinds of reference that we need to worry about:

**Additional fuzzable interfaces** - if an interface method can return a
pending_remote<> or take a pending_receiver<> to an interface Foo, then we
want our fuzzer to fuzz those interfaces too.

Here we would want to add `blink.mojom.Blob.RemoteAction` and
`blink.mojom.BlobURLStore.AssociatedRemoteAction` to the possible actions
that our fuzzer protobufs can take.

**Renderer-hosted interfaces** - if an interface method takes a pending_remote<>
(or returns a pending_receiver<>), then we'll also want to add response handling
to our fuzzer. This lets the fuzzer send fuzzer-side implementations of mojo
interfaces, and handle fuzzing the values returned if those methods are called.

Here we can see `blink.mojom.ProgressClient` is needed, but we can also see that
we pass `blink.mojom.DataElement` structures to `BlobRegistry.Register`. These
can contain `remote<blink.mojom.Blob>`, so we also need to support
`blink.mojom.Blob`.

These are handled similarly to the `RemoteAction`s, but the type that we need to
add to our proto is instead `blink.mojom.ProgressClient.ReceiverAction`, and so
on.

We can continue applying this logic recursively to all of the interfaces that
might be accessed - this comes down to a question of what dependencies are most
likely to be important in getting good coverage, so the later step of examining
code coverage may also help in guiding the addition of new interfaces here.

`blob_registry_mojolpm_fuzzer.proto` illustrates how these responses can be added
to the testcase proto.

## Start fuzzing

Once the fuzzer is up and running, we probably want to remove dcheck_always_on.

```
enable_mojom_fuzzer = true
is_asan = true
is_component_build = true
is_debug = false
optimize_for_fuzzing = true
use_remoteexec = true
use_libfuzzer = true
```

The reason for this is that while DCHECKs are often useful when fuzzing (and a
good indication of potential bugs), the Mojo serialization code often contains
quite a few DCHECKs, and our fuzzer is essentially serializing untrusted data
before it can deserialize that data on the Browser-process side. This means
that we can easily get blocked by a "completely valid" DCHECK during
serialisation that a compromised renderer would bypass. Removing DCHECKs will
sometimes let the fuzzer continue in these situations, and will reduce spurious
results, but if your fuzzer doesn't trigger any of these cases it may be
beneficial to also fuzz with DCHECKs enabled. We'll discuss this below under
[triage](#triage-notes).

If your coverage isn't going up at all, then you've probably made a mistake and
it likely isn't managing to actually interact with the interface you're trying
to fuzz - try using the code coverage output from the next step to debug what's
going wrong.


## (Optional) Run coverage

In many cases it's useful to check the code coverage to see if we can benefit
from adding some manual testcases to get deeper coverage. For this example I
used the following gn arguments and command:

```
enable_mojom_fuzzer = true
is_component_build = false
is_debug = false
use_clang_coverage = true
use_remoteexec = true
use_libfuzzer = true
```

```
python tools/code_coverage/coverage.py code_cache_host_mojolpm_fuzzer -b out/Coverage -o ManualReport -c "out/Coverage/code_cache_host_mojolpm_fuzzer -ignore_timeouts=1 -timeout=4 -runs=0 /dev/shm/corpus" -f content
```

With the CodeCacheHost, looking at the coverage after a few hours we could see
that there's definitely some room for improvement:

```c++
/* 55       */ absl::optional<GURL> GetSecondaryKeyForCodeCache(const GURL& resource_url,
/* 56 53.6k */ int render_process_id) {
/* 57 53.6k */    if (!resource_url.is_valid() || !resource_url.SchemeIsHTTPOrHTTPS())
/* 58 53.6k */      return absl::nullopt;
/* 59 0     */
/* 60 0     */    GURL origin_lock =
/* 61 0     */        ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
/* 62 0     */            render_process_id);
```

## (Optional) Improve corpus manually

It's fairly easy to improve the corpus manually, since our corpus files are just
protobuf files that describe the sequence of interface calls to make.

There are a couple of approaches that we can take here - we'll try building a
small manual seed corpus that we'll use to kick-start our fuzzer. Since it's
easier to edit text protos, MojoLPM can automatically convert our seed corpus
from text protos to binary protos during the build, making this slightly less
painful for us, and letting us store our corpus in-tree in a readable format.

So, we'll create a new folder to hold this seed corpus, and craft our first
file:

```
actions {
  new_code_cache_host {
    id: 1
    render_process_id: 0
    origin_id: ORIGIN_A
  }
}
actions {
  code_cache_host_remote_action {
    id: 1
    m_did_generate_cacheable_metadata {
      m_cache_type: CodeCacheType_kJavascript
      m_url {
        new {
          id: 1
          m_url: "http://aaa.com/test"
        }
      }
      m_data {
        new {
          id: 1
          m_bytes {
          }
        }
      }
      m_expected_response_time {
      }
    }
  }
}
sequences {
  action_indexes: 0
  action_indexes: 1
}
sequence_indexes: 0
```

We can then add some new entries to our build target to have the corpus
converted to binary proto directly during build.

```
  testcase_proto_kind = "content.fuzzing.code_cache_host.proto.Testcase"

  seed_corpus_sources = [
    "code_cache_host_mojolpm_fuzzer_corpus/did_generate_cacheable_metadata.textproto",
  ]
```

If we now run a new coverage report using this single file seed corpus:
(note that the binary corpus files will be output in your output directory, in
this case code_cache_host_mojolpm_fuzzer_seed_corpus.zip):

```
autoninja -C out/Coverage chrome
rm -rf /tmp/corpus; mkdir /tmp/corpus; unzip out/Coverage/code_cache_host_mojolpm_fuzzer_seed_corpus.zip -d /tmp/corpus
python tools/code_coverage/coverage.py code_cache_host_mojolpm_fuzzer -b out/Coverage -o ManualReport -c "out/Coverage/code_cache_host_mojolpm_fuzzer -ignore_timeouts=1 -timeout=4 -runs=0 /tmp/corpus" -f content
```

We can see that we're now getting some more coverage:

```c++
/* 118   */ void CodeCacheHostImpl::DidGenerateCacheableMetadata(
/* 119   */     blink::mojom::CodeCacheType cache_type,
/* 120   */     const GURL& url,
/* 121   */     base::Time expected_response_time,
/* 122 2 */       mojo_base::BigBuffer data) {
/* 123 2 */     if (!url.SchemeIsHTTPOrHTTPS()) {
/* 124 0 */       mojo::ReportBadMessage("Invalid URL scheme for code cache.");
/* 125 0 */       return;
/* 126 0 */     }
/* 127 2 */
/* 128 2 */     DCHECK_CURRENTLY_ON(BrowserThread::UI);
/* 129 2 */
/* 130 2 */     GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
/* 131 2 */     if (!code_cache)
/* 132 0 */       return;
/* 133 2 */
/* 134 2 */     absl::optional<GURL> origin_lock =
/* 135 2 */         GetSecondaryKeyForCodeCache(url, render_process_id_);
/* 136 2 */     if (!origin_lock)
/* 137 0 */       return;
/* 138 2 */
/* 139 2 */     code_cache->WriteEntry(url, *origin_lock, expected_response_time,
/* 140 2 */                            std::move(data));
/* 141 2 */ }
```

Much better!

## Triage notes

MojoLPM fuzzers have a number of common failure modes that are fairly easy to
distinguish from real bugs in the implementation being fuzzed.

The first of these is any crash on the `fuzzer_thread`. Code in the
implementation should never, under any circumstances be running on this thread,
so any crash on this thread is the result of a bug in the fuzzer itself, or
one of the other causes mentioned below.

In AddressSanitizer builds this case can be automatically identified by
additional instrumentation, which is implemented as part of
`content::mojolpm::FuzzerEnvironment` but will need to be duplicated for fuzzers
in other areas of the codebase. This instrumentation prints additional output as
part of the ASan report, and should make the fuzzer exit cleanly for these false
positives so that further instrumentation should ignore these crashes.

```
MojoLPM: FALSE POSITIVE
This crash occurred on the fuzzer thread, so it is a false positive and
does not represent a security issue. In MojoLPM, the fuzzer thread
represents the unprivileged renderer process.
```

The second is DCHECK or other failures during Mojo serialization. Various traits
assert that they are serializing reasonable values - since we need to reuse this
serialization code in the fuzzer to produce input to the implementation, we can
trigger these on the `fuzzer_thread` while processing input to send to the
implementation.

The example ASAN error output below illustrates an example of both of these
cases - the error happens on the `fuzzer_thread`, and during serialization.

```
==2940792==ERROR: AddressSanitizer: ILL on unknown address 0x7fbd9391d0f9 (pc 0x7fbd9391d0f9 bp 0x7fbd24deb3e0 sp 0x7fbd24deb3e0 T5)
    #0 0x7fbd9391d0f9 in unsigned int base::internal::CheckOnFailure::HandleFailure<unsigned int>() base/numerics/safe_conversions_impl.h:122:5
    #1 0x7fbd9391ba78 in unsigned int base::internal::checked_cast<unsigned int, base::internal::CheckOnFailure, unsigned long>(unsigned long) base/numerics/safe_conversions.h:114:16
    #2 0x7fbd9391ba28 in mojo::StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView, mojo_base::internal::BigBufferSharedMemoryRegion>::size(mojo_base::internal::BigBufferSharedMemoryRegion const&) mojo/public/cpp/base/big_buffer_mojom_traits.cc:17:10
    #3 0x7fbd7f62fc2e in mojo::internal::Serializer<mojo_base::mojom::BigBufferSharedMemoryRegionDataView, mojo_base::internal::BigBufferSharedMemoryRegion>::Serialize(mojo_base::internal::BigBufferSharedMemoryRegion&, mojo::internal::Buffer*, mojo_base::mojom::internal::BigBufferSharedMemoryRegion_Data::BufferWriter*, mojo::internal::SerializationContext*) gen/mojo/public/mojom/base/big_buffer.mojom-shared.h:182:23
...
    #41 0x7fbd955376e8 in base::RunLoop::Run() base/run_loop.cc:124:14
    #42 0x7fbd95707f83 in base::Thread::Run(base::RunLoop*) base/threading/thread.cc:311:13
    #43 0x7fbd95708427 in base::Thread::ThreadMain() base/threading/thread.cc:382:3
    #44 0x7fbd957dfb40 in base::(anonymous namespace)::ThreadFunc(void*) base/threading/platform_thread_posix.cc:81:13
    #45 0x7fbd403866b9 in start_thread /build/glibc-LK5gWL/glibc-2.23/nptl/pthread_create.c:333
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: ILL (/mnt/scratch0/clusterfuzz/bot/builds/chromium-browser-libfuzzer_linux-release-asan_ae530a86793cd6b8b56ce9af9159ac101396e802/revisions/libfuzzer-linux-release-807440/libmojo_base_shared_typemap_traits.so+0x190f9)
Thread T5 (fuzzer_thread) created by T0 here:
    #0 0x56433ef70b3a in pthread_create third_party/llvm/compiler-rt/lib/asan/asan_interceptors.cpp:214:3
...
    #14 0x56433f15380c in main third_party/libFuzzer/src/FuzzerMain.cpp:19:10
    #15 0x7fbd3c38a82f in __libc_start_main /build/glibc-LK5gWL/glibc-2.23/csu/libc-start.c:291
==2940792==ABORTING
```

## Debugging tips

`LOG()` statements don't print while running the fuzzer, but printing to
`std::cout` should work. NOTE(caraitto): This is likely due to the lack of
`--enable-logging=stderr`, but `LOG()` only worked in certain contexts when
adding that to the command line during `FuzzerEnvironment` setup. `CHECK()`
should work though.

[`google::protobuf::TextFormat::PrintToString()`] can be used to dump the
contents of the current testcase proto. This can be useful to help inspect the
contents of individual crash testcase files, as you can invoke the fuzzer with a
crash testcase instead of a corpus directory, and then `PrintToString()` can
print out a string representation of the crash testcase file. This can be easier
than trying to use command-line protobuf printing tools as these may require
listing all .proto schema files used, including the many transitive includes.

The [`mojolpm::Context`] global singleton stores objects like Mojo remotes and
return values of Mojo methods. It can help connect custom action implementations
with the generated code. Objects are keyed by the type of object and a numeric
ID that starts at 1 -- for instance, this is how the
`code_cache_host_remote_action` above knows to use the specific remote created
by the `new_code_cache_host` -- they both use the `id` of 1.

By changing the [`MOJOLPM_DBG`] `#define` to 1, a number of `mojolpm::Context`
debug logging sites will be enabled. It's also possible to add logging to the
generated code by altering the [generated code templates].

Code coverage, as mentioned above, can also be a good tool to determine how far
into the code the fuzzer is exploring. It's possible to run coverage on the seed
corpus to see how much code gets covered initially, or run the fuzzer normally
(non-coverage run) for a few minutes / hours, starting with the seed corpus,
then run coverage using the resultant corpus directory to see how much
additional coverage the fuzzer was able to gain through exploration. (Coverage
runs don't produce new testcases). You may want to periodically monitor code
coverage to ensure that product code changes don't result in loss of fuzzer
coverage. However, if you just want to see if a particular line gets covered, it
might be faster to add a print or `CHECK()` at that line and run the fuzzer.

[markbrand@google.com]:mailto:markbrand@google.com?subject=[MojoLPM%20Help]:%20&cc=fuzzing@chromium.org
[libfuzzer]: https://source.chromium.org/chromium/chromium/src/+/main:testing/libfuzzer/getting_started.md
[Protocol Buffers]: https://developers.google.com/protocol-buffers/docs/cpptutorial
[libprotobuf-mutator]: https://source.chromium.org/chromium/chromium/src/+/main:testing/libfuzzer/libprotobuf-mutator.md
[testing in Chromium]: https://source.chromium.org/chromium/chromium/src/+/main:docs/testing/testing_in_chromium.md
[interfaces]: https://source.chromium.org/search?q=interface%5Cs%2B%5Cw%2B%5Cs%2B%7B%20f:%5C.mojom$%20-f:test
[`google::protobuf::TextFormat::PrintToString()`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/protobuf/src/google/protobuf/text_format.h;l=92;drc=b8644e8bc11097152e648510ca97dad0a20c1aae
[`mojolpm::Context`]: https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/tools/fuzzers/mojolpm.cc;l=85;drc=6f3f85b321146cfc0f9eb81a74c7c2257821461e
[`MOJOLPM_DBG`]: https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/tools/fuzzers/mojolpm.h;l=25;drc=6f3f85b321146cfc0f9eb81a74c7c2257821461e
[generated code templates]: https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/tools/bindings/generators/mojolpm_templates/;drc=af0878e4870444f6347f915a5f24f438085913f6