# Modifying Session History Serialization

*Note: Please expand these steps as needed. See also
[NavigationEntryImpl](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/navigation_entry_impl.h;drc=23e90a11b9094fe2682083445f14abbaea9bef48;l=524)
comments for how to save and restore values outside of PageState, which is less
common.*

## Overview

The following (non-exhaustive) steps are required to add new values to the
PageState data serialization process, which is used for saving and restoring
values in session restore across different versions of Chromium, as well as tab
restore and tab duplication within a running instance of Chromium.

Note that changing the serialization format is **high risk** and should be
approached carefully.  Mistakes or missed steps can cause backwards
compatibility problems, because the effects can continue to live on disk between
different versions of Chromium. For this reason, it is important to make these
changes carefully, minimizing the risk of reverts (*e.g.*, not as part of much
larger CLs) or other back-and-forth between formats (*e.g.*, not as part of an
A/B experiment).

Please ask the CL reviewer to also consult this checklist to ensure no steps
have been missed.

## PageState Modification Checklist

- Update NavigationEntryImpl's
  [`RecursivelyGenerateFrameEntries`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/navigation_entry_impl.cc;drc=23e90a11b9094fe2682083445f14abbaea9bef48;l=59)
  and
  [`RecursivelyGenerateFrameState`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/navigation_entry_impl.cc;drc=23e90a11b9094fe2682083445f14abbaea9bef48;l=132)
  in `content/browser/renderer_host/navigation_entry_impl.cc` to save and
  restore the new value from appropriate locations in Chromium.
- Update any member declaration comments for the new value in
    `content/browser/renderer_host/frame_navigation_entry.h`, indicating if the
    new value is persisted if necessary (*e.g.*
    [here](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/frame_navigation_entry.h;drc=23e90a11b9094fe2682083445f14abbaea9bef48;l=286)).
- Update `third_party/blink/public/mojom/page_state/page_state.mojom`,
  following the current instructions and warnings in the comments. These
  include (but are not limited to):
    - Search for [`Next
    MinVersion`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/page_state/page_state.mojom;drc=047c7dc4ee1ce908d7fea38ca063fa2f80f92c77;l=43)
    in the comments and increment it.
    - Add the new value under the appropriate section, remembering to
      increment the [`Next
      Ordinal`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/page_state/page_state.mojom;drc=047c7dc4ee1ce908d7fea38ca063fa2f80f92c77;l=108)
      value for that section. Include `[MinVersion=<N>]` in front of the
      mojom declaration for the newly added value. *E.g.* if adding a value to
      `ExplodedFrameState`, update the `Next Ordinal` value for the `struct
      FrameState` section.
- Add the new element to
  [`ExplodedFrameState`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/page_state/page_state_serialization.h;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=36)
  in
  `third_party/blink/public/common/page_state/page_state_serialization.h`.
- Update the value of
  [`kCurrentVersion`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=203)
  in `third_party/blink/common/page_state/page_state_serialization.cc`, and
  modify the comment above to explain what’s new in this version.
- Update the
  [`WriteFrameState`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=601)
  and
  [`ReadFrameState`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=804)
  functions in `third_party/blink/common/page_state/page_state_serialization.cc`
  to save and restore the new value in the persistence format.
- Update
  [`ExplodedFrameState::assign()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=953)
  to copy the new element so that its copy constructor and assignment operator
  works as expected.
- Update the [`ExplodedFrameState`-specialization of
  `ExpectEquality()`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization_unittest.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=86)
  in `third_party/blink/common/page_state/page_state_serialization_unittest.cc`.
- Update
  [`PopulateFrameStateForBackwardsCompatTest`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization_unittest.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=169),
  for cases where the `version` value equals/exceeds the new `kCurrentVersion`
  set earlier.
- Update
  [`PopulateFrameState`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization_unittest.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=124)
  to include the newly-added value.
- Add a [backwards compatibility test](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization_unittest.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=635),
  `TEST_F(PageStateSerializationTest, BackwardsCompat_v<NN>)`, where `<NN>`
  is replaced by the new kCurrentVersion value.
- Add a new baseline data file
  `third_party/blink/common/page_state/test_data/serialized_v<NN>.dat`, using
  the following steps:
    - In
      `third_party/blink/common/page_state/page_state_serialization_unittest.cc`,
      find the
      [`#if 0`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/page_state/page_state_serialization_unittest.cc;drc=f98aa0661150d5b2d2ed40562fb398e049172549;l=518)
      directive before `TEST_F(PageStateSerializationTest,
      DumpExpectedPageStateForBackwardsCompat)`, and temporarily change it to
      `#if 1`.
    - Compile `blink_common_unittests` then run with
   `--gtest_filter=PageStateSerializationTest.DumpExpectedPageStateForBackwardsCompat`
    - This will create a file `expected.dat` in the temp directory (*e.g.*
      `/tmp` on Linux). Copy this file to
      `third_party/blink/common/page_state/test_data/serialized_v<NN>.dat`
      and run `git add`.
    - Remember to undo the `#if 1` change made to
      `page_state_serialization_unittest.cc` above once the new baseline data
      has been created.

## Example CLs modifying the format:

- [https://chromium-review.googlesource.com/c/chromium/src/+/1679162](https://chromium-review.googlesource.com/c/chromium/src/+/1679162)
- [https://chromium-review.googlesource.com/c/chromium/src/+/4092432](https://chromium-review.googlesource.com/c/chromium/src/+/4092432)

## Example bugs from missing steps:

- [1405812](https://crbug.com/1405812)
