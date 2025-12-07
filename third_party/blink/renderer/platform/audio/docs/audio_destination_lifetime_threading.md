# AudioDestination: Lifetime and Threading

This document clarifies the lifetime and threading model of the
`AudioDestination` class, which is an audio sink that sits between the WebAudio
module and the underlying media renderer.

## Object Ownership and Lifetime

The lifetime of an `AudioDestination` is ultimately tied to a `Document`. The
ownership chain is as follows:

  `Document` -> `BaseAudioContext` -> `AudioDestinationNode`
  -> `AudioDestinationHandler` -> `AudioDestination`

When the `Document` is destroyed, this entire chain is torn down.
`AudioDestination` is ref-counted (`ThreadSafeRefCounted`) because it is
accessed from multiple threads, ensuring it remains alive until all users
have released their references.

## Relationship with `WebLocalFrame`

`AudioDestination` creates and owns a `WebAudioDevice`
(`RendererWebAudioDeviceImpl`), which needs to interact with the `Frame`.
However, it does NOT hold a raw pointer. Instead, it holds a
`blink::LocalFrameToken`.

This is key to the lifetime question: `AudioDestination` *can* outlive the
`WebLocalFrame` to which it was associated. When the `WebAudioDevice` needs
to access the frame (e.g., to create a `SpeechRecognitionClient`), it uses
the token. If the frame has already been destroyed, `WebLocalFrame::From()`
will safely return `nullptr`, and the feature will be gracefully disabled.
This prevents use-after-free crashes.

## Threading Models

`AudioDestination` has two threading models for audio rendering. This is NOT
about object ownership, but about which thread drives the audio data pump.

1.  **Single-Thread Model (Default):** An `AudioDeviceThread` drives the
    rendering callbacks from the underlying audio hardware.

2.  **Dual-Thread Model (with AudioWorklet):** A dedicated
    `AudioWorkletThread` is responsible for pulling audio data from the
    WebAudio graph.

In both models, the ultimate lifetime of the `AudioDestination` object is
determined by the ownership chain from the `Document`, not by the thread it
runs on.
