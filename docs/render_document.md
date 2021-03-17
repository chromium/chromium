# What is RenderDocument?

## TL;DR

Chrome currently switches to a new RenderFrameHost
when loading a new document
if the render process is different to the  previous one.
The RenderDocument project is about making the switch to happen unconditionally.
This:

* Eliminates the logic for navigating inside the same RenderFrameHost
* Makes RenderFrameHost in the browser process 1:1 with the Document.
* Prevents security bugs,
  e.g. reusing the data/capabilities from the wrong document.

## Details

Previously when we navigate a frame from one page to another,
the second page may appear in a new RenderFrame
or we may reuse the existing RenderFrame to load the second page.
Which happens depends on many things,
including which site-isolation policy we are following
and whether the pages are from the same site or not.
With RenderDocument,
the second page will always use a new RenderFrame
(excluding navigation within a document).

Also when reloading a crashed frame
we reused the browser-side RenderFrameHost.
With RenderDocument we create a new RenderFrameHost
for crashed frames.

## Read more

https://crbug.com/936696

[design doc](https://docs.google.com/document/d/1C2VKkFRSc0kdmqjKan1G4NlNlxWZqE4Wam41FNMgnmA)

[high-level view of the work needed](https://docs.google.com/document/d/1UzVOmTj2IJ0ecz7CZicTK6ow2rr9wgLTGfY5hjyLmT4)

[discussion of how we can land it safely](https://docs.google.com/document/d/1ZHWWEYT1L5Zgh2lpC7DHXXZjKcptI877KKOqjqxE2Ns)

# Stages

We have 3 stages that are behind flags.

1. crashed-frames:
  A new `RenderFrameHost` is used for reloading a crashed document.
2. subframes:
  A new `RenderFrameHost` is used for every nested document.
3. main frames:
  A new `RenderFrameHost` is used for every document.

# Test changes

## RenderFrameHost reference becomes invalid

Enabling this for subframes and main frames causes many tests to fail.
It is common for tests to get a reference to a RenderFrameHost
and then navigate that frame,
assuming that the reference will remain valid.
This assumption is no longer valid.
The test needs to get a reference to the new RenderFrameHost,
e.g. by traversing the frame tree again.
